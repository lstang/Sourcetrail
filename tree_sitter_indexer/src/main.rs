use std::collections::{HashMap, HashSet};
use std::fs;
use std::path::Path;
use rusqlite::{params, Connection};
use tree_sitter::{Node, Parser};

// Sourcetrail Node Kinds
const NODE_TYPE: i32 = 1 << 1;
const NODE_NAMESPACE: i32 = 1 << 4;
const NODE_STRUCT: i32 = 1 << 6;
const NODE_CLASS: i32 = 1 << 7;
const NODE_INTERFACE: i32 = 1 << 8;
const NODE_FUNCTION: i32 = 1 << 12;
const NODE_METHOD: i32 = 1 << 13;
const NODE_FILE: i32 = 1 << 18;

// Sourcetrail Edge Types
const EDGE_MEMBER: i32 = 1 << 0;     // 1
const EDGE_CALL: i32 = 1 << 3;       // 8

// Sourcetrail Definition Kinds
const DEFINITION_NONE: i32 = 0;
const DEFINITION_EXPLICIT: i32 = 2;

// Location Types
const LOCATION_TOKEN: i32 = 0;
const LOCATION_SCOPE: i32 = 1;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 2 || args[1] == "--help" || args[1] == "-h" {
        println!("SourcetrailTreeSitterIndexer - Multi-language source code indexer using Tree-sitter");
        println!("Usage: SourcetrailTreeSitterIndexer index --source-file-path <file> --database-file-path <db> [--shallow] [--verbose] [--clear]");
        println!("       SourcetrailTreeSitterIndexer check-environment");
        return;
    }

    if args[1] == "check-environment" {
        println!("Tree-sitter environment is ready (native Rust indexer with Rust, Python, C, C++, JS, Go, C#, Java support).");
        return;
    }

    if args[1] != "index" {
        eprintln!("Unknown command: {}", args[1]);
        std::process::exit(1);
    }

    let mut source_file_path = String::new();
    let mut database_file_path = String::new();
    let mut clear_db = false;

    let mut i = 2;
    while i < args.len() {
        if args[i] == "--source-file-path" && i + 1 < args.len() {
            source_file_path = args[i + 1].clone();
            i += 2;
        } else if args[i] == "--database-file-path" && i + 1 < args.len() {
            database_file_path = args[i + 1].clone();
            i += 2;
        } else if args[i] == "--clear" {
            clear_db = true;
            i += 1;
        } else {
            i += 1;
        }
    }

    if source_file_path.is_empty() || database_file_path.is_empty() {
        eprintln!("Missing required arguments: --source-file-path or --database-file-path");
        std::process::exit(1);
    }

    if let Err(e) = index_file(&source_file_path, &database_file_path, clear_db) {
        eprintln!("ERROR: {}", e);
        std::process::exit(1);
    }
}

fn get_language_for_extension(ext: &str) -> Option<(&'static str, tree_sitter::Language)> {
    match ext.to_lowercase().as_str() {
        "rs" => Some(("rust", tree_sitter_rust::LANGUAGE.into())),
        "py" => Some(("python", tree_sitter_python::LANGUAGE.into())),
        "c" | "h" => Some(("c", tree_sitter_c::LANGUAGE.into())),
        "cpp" | "hpp" | "cc" | "cxx" | "hxx" => Some(("cpp", tree_sitter_cpp::LANGUAGE.into())),
        "js" | "jsx" | "mjs" | "cjs" => Some(("javascript", tree_sitter_javascript::LANGUAGE.into())),
        "go" => Some(("go", tree_sitter_go::LANGUAGE.into())),
        "cs" => Some(("csharp", tree_sitter_c_sharp::LANGUAGE.into())),
        "java" => Some(("java", tree_sitter_java::LANGUAGE.into())),
        _ => None,
    }
}

fn format_serialized_name(full_name: &str, postfix: &str) -> String {
    if full_name.starts_with("/\tm") || (full_name.starts_with('<') && full_name.ends_with('>')) {
        return full_name.to_string();
    }

    let parts: Vec<&str> = full_name.split('.').collect();
    let mut sb = String::from(".\tm");
    for (idx, part) in parts.iter().enumerate() {
        if idx > 0 {
            sb.push_str("\tn");
        }
        sb.push_str(part);
        sb.push_str("\ts");
        if idx == parts.len() - 1 {
            sb.push_str("\tp");
            sb.push_str(postfix);
        } else {
            sb.push_str("\tp");
        }
    }
    sb
}

struct DbWriter<'a> {
    tx: &'a rusqlite::Transaction<'a>,
    file_node_id: i32,
    next_element_id: i32,
    next_location_id: i32,
    symbol_map: HashMap<String, i32>,
    edge_set: HashSet<(i32, i32, i32)>,
}

impl<'a> DbWriter<'a> {
    fn record_node(&mut self, name_key: &str, node_type: i32, def_kind: i32, postfix: &str) -> Result<i32, rusqlite::Error> {
        let key = format!("{}{}", name_key, postfix);
        if let Some(&id) = self.symbol_map.get(&key) {
            if def_kind == DEFINITION_EXPLICIT {
                self.tx.execute("UPDATE symbol SET definition_kind = 2 WHERE id = ?1 AND definition_kind < 2;", params![id])?;
            }
            return Ok(id);
        }

        let serialized_name = format_serialized_name(name_key, postfix);
        let id = self.next_element_id;
        self.next_element_id += 1;

        self.tx.execute("INSERT INTO element(id) VALUES (?1);", params![id])?;
        self.tx.execute("INSERT INTO node(id, type, serialized_name) VALUES (?1, ?2, ?3);", params![id, node_type, serialized_name])?;
        self.tx.execute("INSERT INTO symbol(id, definition_kind) VALUES (?1, ?2);", params![id, def_kind])?;

        self.symbol_map.insert(key, id);
        Ok(id)
    }

    fn record_location(&mut self, elem_id: i32, start_line: usize, start_col: usize, end_line: usize, end_col: usize, loc_type: i32) -> Result<(), rusqlite::Error> {
        let loc_id = self.next_location_id;
        self.next_location_id += 1;

        self.tx.execute(
            "INSERT INTO source_location(id, file_node_id, start_line, start_column, end_line, end_column, type) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7);",
            params![loc_id, self.file_node_id, start_line as i32, start_col as i32, end_line as i32, end_col as i32, loc_type],
        )?;
        self.tx.execute(
            "INSERT INTO occurrence(element_id, source_location_id) VALUES (?1, ?2);",
            params![elem_id, loc_id],
        )?;
        Ok(())
    }

    fn record_edge(&mut self, source_id: i32, target_id: i32, edge_type: i32) -> Result<i32, rusqlite::Error> {
        if source_id <= 0 || target_id <= 0 || source_id == target_id {
            return Ok(0);
        }
        let edge_key = (source_id, target_id, edge_type);
        if self.edge_set.contains(&edge_key) {
            return Ok(0);
        }
        self.edge_set.insert(edge_key);

        let edge_id = self.next_element_id;
        self.next_element_id += 1;

        self.tx.execute("INSERT INTO element(id) VALUES (?1);", params![edge_id])?;
        self.tx.execute(
            "INSERT INTO edge(id, type, source_node_id, target_node_id) VALUES (?1, ?2, ?3, ?4);",
            params![edge_id, edge_type, source_id, target_id],
        )?;
        Ok(edge_id)
    }
}

fn index_file(source_file_path: &str, database_file_path: &str, clear_db: bool) -> Result<(), Box<dyn std::error::Error>> {
    let source_path = Path::new(source_file_path).canonicalize()?;
    let mut source_str = source_path.to_string_lossy().replace('\\', "/");
    if source_str.starts_with("//?/") {
        source_str = source_str[4..].to_string();
    }
    let ext = source_path.extension().and_then(|e| e.to_str()).unwrap_or("");

    let (lang_name, ts_lang) = match get_language_for_extension(ext) {
        Some(res) => res,
        None => ("text", tree_sitter_c::LANGUAGE.into()),
    };

    let bytes = fs::read(&source_path)?;
    let code = match String::from_utf8(bytes) {
        Ok(s) => s,
        Err(e) => String::from_utf8_lossy(e.as_bytes()).into_owned(),
    };
    let line_count = code.lines().count().max(1) as i32;

    let mut parser = Parser::new();
    parser.set_language(&ts_lang)?;
    let tree = parser.parse(&code, None).ok_or("Failed to parse code with tree-sitter")?;
    let root = tree.root_node();

    let mut conn = Connection::open(database_file_path)?;

    conn.execute_batch(
        "
        CREATE TABLE IF NOT EXISTS meta(id INTEGER PRIMARY KEY, key TEXT, value TEXT);
        CREATE TABLE IF NOT EXISTS element(id INTEGER PRIMARY KEY);
        CREATE TABLE IF NOT EXISTS element_component(id INTEGER PRIMARY KEY, element_id INTEGER, type INTEGER, data TEXT);
        CREATE TABLE IF NOT EXISTS edge(id INTEGER PRIMARY KEY, type INTEGER NOT NULL, source_node_id INTEGER NOT NULL, target_node_id INTEGER NOT NULL);
        CREATE TABLE IF NOT EXISTS node(id INTEGER PRIMARY KEY, type INTEGER NOT NULL, serialized_name TEXT);
        CREATE TABLE IF NOT EXISTS symbol(id INTEGER PRIMARY KEY, definition_kind INTEGER NOT NULL);
        CREATE TABLE IF NOT EXISTS file(id INTEGER PRIMARY KEY, path TEXT, language TEXT, modification_time TEXT, indexed INTEGER, complete INTEGER, line_count INTEGER);
        CREATE TABLE IF NOT EXISTS filecontent(id INTEGER PRIMARY KEY, content TEXT);
        CREATE TABLE IF NOT EXISTS local_symbol(id INTEGER PRIMARY KEY, name TEXT);
        CREATE TABLE IF NOT EXISTS source_location(id INTEGER PRIMARY KEY, file_node_id INTEGER, start_line INTEGER, start_column INTEGER, end_line INTEGER, end_column INTEGER, type INTEGER);
        CREATE TABLE IF NOT EXISTS occurrence(element_id INTEGER NOT NULL, source_location_id INTEGER NOT NULL, PRIMARY KEY(element_id, source_location_id));
        CREATE TABLE IF NOT EXISTS component_access(node_id INTEGER PRIMARY KEY, type INTEGER NOT NULL);
        CREATE TABLE IF NOT EXISTS error(id INTEGER PRIMARY KEY, message TEXT, fatal INTEGER NOT NULL, indexed INTEGER NOT NULL, translation_unit TEXT);
        ",
    )?;

    if clear_db {
        conn.execute_batch(
            "DELETE FROM occurrence; DELETE FROM source_location; DELETE FROM edge; DELETE FROM symbol; DELETE FROM file; DELETE FROM filecontent; DELETE FROM node; DELETE FROM element;",
        )?;
    }

    let tx = conn.transaction()?;

    let next_element_id: i32 = tx
        .query_row("SELECT COALESCE(MAX(id), 0) FROM element;", [], |r| r.get(0))
        .unwrap_or(0)
        + 1;

    let next_location_id: i32 = tx
        .query_row("SELECT COALESCE(MAX(id), 0) FROM source_location;", [], |r| r.get(0))
        .unwrap_or(0)
        + 1;

    // Metadata
    tx.execute(
        "INSERT OR IGNORE INTO meta(id, key, value) VALUES (1, 'db_version', '25'), (2, 'app_version', '2021.4.19');",
        [],
    )?;

    // Mod time formatted yyyy-MM-dd HH:mm:ss
    let mod_time = if let Ok(meta) = fs::metadata(&source_path) {
        if let Ok(mtime) = meta.modified() {
            let dt: chrono::DateTime<chrono::Local> = mtime.into();
            dt.format("%Y-%m-%d %H:%M:%S").to_string()
        } else {
            "2026-10-10 12:00:00".to_string()
        }
    } else {
        "2026-10-10 12:00:00".to_string()
    };

    // File Node
    let file_node_id = next_element_id;

    tx.execute("INSERT INTO element(id) VALUES (?1);", params![file_node_id])?;
    tx.execute(
        "INSERT INTO node(id, type, serialized_name) VALUES (?1, ?2, ?3);",
        params![file_node_id, NODE_FILE, format!("/\tm{}\ts\tp", source_str)],
    )?;
    tx.execute(
        "INSERT OR REPLACE INTO file(id, path, language, modification_time, indexed, complete, line_count) VALUES (?1, ?2, ?3, ?4, 1, 1, ?5);",
        params![file_node_id, source_str, lang_name, mod_time, line_count],
    )?;
    tx.execute(
        "INSERT OR REPLACE INTO filecontent(id, content) VALUES (?1, ?2);",
        params![file_node_id, code],
    )?;

    let mut writer = DbWriter {
        tx: &tx,
        file_node_id,
        next_element_id: next_element_id + 1,
        next_location_id,
        symbol_map: HashMap::new(),
        edge_set: HashSet::new(),
    };

    walk_node(root, &code, "", 0, &mut writer)?;

    drop(writer);
    tx.commit()?;
    Ok(())
}

fn get_node_text<'a>(node: Node<'a>, code: &'a str) -> &'a str {
    &code[node.byte_range()]
}

fn find_child_by_field_or_kind<'a>(node: Node<'a>, field: &str, kind: &str) -> Option<Node<'a>> {
    if let Some(child) = node.child_by_field_name(field) {
        return Some(child);
    }
    let mut cursor = node.walk();
    for child in node.children(&mut cursor) {
        if child.kind() == kind {
            return Some(child);
        }
    }
    None
}

fn walk_node<'a>(
    node: Node<'a>,
    code: &'a str,
    current_scope: &str,
    parent_symbol_id: i32,
    writer: &mut DbWriter,
) -> Result<(), Box<dyn std::error::Error>> {
    let kind = node.kind();

    // 1. Module / Namespace / Package
    let is_module = matches!(
        kind,
        "mod_item" | "namespace_definition" | "namespace_declaration" | "package_clause"
    );

    if is_module {
        let name_node = find_child_by_field_or_kind(node, "name", "identifier");
        if let Some(name_n) = name_node {
            let name = get_node_text(name_n, code);
            let full_name = if current_scope.is_empty() {
                name.to_string()
            } else {
                format!("{}.{}", current_scope, name)
            };

            let mod_id = writer.record_node(&full_name, NODE_NAMESPACE, DEFINITION_EXPLICIT, "")?;
            let start_pt = name_n.start_position();
            let end_pt = name_n.end_position();
            writer.record_location(mod_id, start_pt.row + 1, start_pt.column + 1, end_pt.row + 1, end_pt.column + 1, LOCATION_TOKEN)?;

            let scope_start = node.start_position();
            let scope_end = node.end_position();
            writer.record_location(mod_id, scope_start.row + 1, scope_start.column + 1, scope_end.row + 1, scope_end.column + 1, LOCATION_SCOPE)?;

            if parent_symbol_id > 0 {
                writer.record_edge(parent_symbol_id, mod_id, EDGE_MEMBER)?;
            }

            let mut cursor = node.walk();
            for child in node.children(&mut cursor) {
                walk_node(child, code, &full_name, mod_id, writer)?;
            }
            return Ok(());
        }
    }

    // 2. Class / Struct / Trait / Interface
    let is_type = matches!(
        kind,
        "struct_item" | "trait_item" | "class_definition" | "class_specifier" | "struct_specifier"
            | "class_declaration" | "interface_declaration" | "type_spec"
    );

    if is_type {
        let name_node = find_child_by_field_or_kind(node, "name", "type_identifier")
            .or_else(|| find_child_by_field_or_kind(node, "name", "identifier"));

        if let Some(name_n) = name_node {
            let name = get_node_text(name_n, code);
            let full_name = if current_scope.is_empty() {
                name.to_string()
            } else {
                format!("{}.{}", current_scope, name)
            };

            let node_kind = if kind.contains("interface") || kind.contains("trait") {
                NODE_INTERFACE
            } else if kind.contains("struct") {
                NODE_STRUCT
            } else {
                NODE_CLASS
            };

            let type_id = writer.record_node(&full_name, node_kind, DEFINITION_EXPLICIT, "")?;
            let start_pt = name_n.start_position();
            let end_pt = name_n.end_position();
            writer.record_location(type_id, start_pt.row + 1, start_pt.column + 1, end_pt.row + 1, end_pt.column + 1, LOCATION_TOKEN)?;

            let scope_start = node.start_position();
            let scope_end = node.end_position();
            writer.record_location(type_id, scope_start.row + 1, scope_start.column + 1, scope_end.row + 1, scope_end.column + 1, LOCATION_SCOPE)?;

            if parent_symbol_id > 0 {
                writer.record_edge(parent_symbol_id, type_id, EDGE_MEMBER)?;
            }

            let mut cursor = node.walk();
            for child in node.children(&mut cursor) {
                walk_node(child, code, &full_name, type_id, writer)?;
            }
            return Ok(());
        }
    }

    // 3. Impl Item (Rust)
    if kind == "impl_item" {
        let trait_node = node.child_by_field_name("trait");
        let type_node = node.child_by_field_name("type");

        let full_name = if let Some(type_n) = type_node {
            let type_text = get_node_text(type_n, code);
            if current_scope.is_empty() {
                type_text.to_string()
            } else {
                format!("{}.{}", current_scope, type_text)
            }
        } else {
            current_scope.to_string()
        };

        let type_id = if !full_name.is_empty() {
            let id = writer.record_node(&full_name, NODE_CLASS, DEFINITION_EXPLICIT, "")?;
            if let Some(trait_n) = trait_node {
                let trait_text = get_node_text(trait_n, code);
                let trait_id = writer.record_node(trait_text, NODE_INTERFACE, DEFINITION_NONE, "")?;
                writer.record_edge(id, trait_id, 1 << 4)?; // EDGE_INHERITANCE
            }
            id
        } else {
            parent_symbol_id
        };

        let mut cursor = node.walk();
        for child in node.children(&mut cursor) {
            walk_node(child, code, &full_name, type_id, writer)?;
        }
        return Ok(());
    }

    // 4. Function / Method
    let is_func = matches!(
        kind,
        "function_item" | "function_definition" | "method_definition" | "method_declaration"
            | "function_declaration" | "constructor_declaration"
    );

    if is_func {
        let name_node = find_child_by_field_or_kind(node, "name", "identifier");
        if let Some(name_n) = name_node {
            let name = get_node_text(name_n, code);
            let full_name = if current_scope.is_empty() {
                name.to_string()
            } else {
                format!("{}.{}", current_scope, name)
            };

            let func_kind = if parent_symbol_id > 0 { NODE_METHOD } else { NODE_FUNCTION };
            let func_id = writer.record_node(&full_name, func_kind, DEFINITION_EXPLICIT, "()")?;

            let start_pt = name_n.start_position();
            let end_pt = name_n.end_position();
            writer.record_location(func_id, start_pt.row + 1, start_pt.column + 1, end_pt.row + 1, end_pt.column + 1, LOCATION_TOKEN)?;

            let scope_start = node.start_position();
            let scope_end = node.end_position();
            writer.record_location(func_id, scope_start.row + 1, scope_start.column + 1, scope_end.row + 1, scope_end.column + 1, LOCATION_SCOPE)?;

            if parent_symbol_id > 0 {
                writer.record_edge(parent_symbol_id, func_id, EDGE_MEMBER)?;
            }

            let mut cursor = node.walk();
            for child in node.children(&mut cursor) {
                walk_node(child, code, &full_name, func_id, writer)?;
            }
            return Ok(());
        }
    }

    // 5. Calls / Invocations
    let is_call = matches!(kind, "call_expression" | "invocation_expression");
    if is_call && parent_symbol_id > 0 {
        let func_node = node.child_by_field_name("function").or_else(|| node.child(0));
        if let Some(func_n) = func_node {
            let callee_name = get_node_text(func_n, code);
            if !callee_name.is_empty() && !callee_name.contains('(') {
                let callee_id = writer.record_node(callee_name, NODE_FUNCTION, DEFINITION_NONE, "()")?;
                let edge_id = writer.record_edge(parent_symbol_id, callee_id, EDGE_CALL)?;
                if edge_id > 0 {
                    let pt = func_n.start_position();
                    let e_pt = func_n.end_position();
                    writer.record_location(edge_id, pt.row + 1, pt.column + 1, e_pt.row + 1, e_pt.column + 1, LOCATION_TOKEN)?;
                }
            }
        }
    }

    // Traverse children
    let mut cursor = node.walk();
    for child in node.children(&mut cursor) {
        walk_node(child, code, current_scope, parent_symbol_id, writer)?;
    }
    Ok(())
}
