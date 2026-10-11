#!/usr/bin/env python3
"""
import_codemap.py - Converts CodeGraph (.codegraph/codegraph.db) and Graphify (graphify-out/graph.json)
databases into Sourcetrail native project (.srctrlprj) and database (.srctrldb) format.
"""

import sys
import os
import sqlite3
import json
import re
import time

def find_repo_root(input_path, file_samples=None):
    norm_path = os.path.abspath(input_path)
    if os.path.isfile(norm_path):
        dir_path = os.path.dirname(norm_path)
    else:
        dir_path = norm_path

    # Check if inside .codegraph or graphify-out
    parent = os.path.dirname(dir_path)
    base = os.path.basename(dir_path).lower()
    if base in ('.codegraph', 'graphify-out', 'graphify'):
        return parent.replace('\\', '/')

    # Check ancestors for .git
    curr = dir_path
    while curr and os.path.dirname(curr) != curr:
        if os.path.exists(os.path.join(curr, '.git')):
            return curr.replace('\\', '/')
        curr = os.path.dirname(curr)

    if file_samples:
        for sample in file_samples:
            candidate = os.path.join(dir_path, sample)
            if os.path.exists(candidate):
                return dir_path.replace('\\', '/')
            candidate_parent = os.path.join(parent, sample)
            if os.path.exists(candidate_parent):
                return parent.replace('\\', '/')
def serialize_name(qname, ns=None, delimiter='.', is_method=False):
    parts = []
    if ns:
        parts.extend(str(ns).replace('::', '.').split('.'))
    if qname:
        parts.extend(str(qname).replace('::', '.').split('.'))
    parts = [p for p in parts if p]
    if not parts:
        parts = [str(qname or 'anonymous')]
    res = (delimiter or '.') + '\tm'
    for i, p in enumerate(parts):
        if i > 0:
            res += '\tn'
        res += p + '\ts\tp'
        if is_method and i == len(parts) - 1:
            res += '()'
    return res
def serialize_file_name(path):
    return '/\tm' + path + '\ts\tp'

KIND_MAP = {
    'class': 128,         # NODE_CLASS
    'struct': 64,         # NODE_STRUCT
    'interface': 256,     # NODE_INTERFACE
    'namespace': 16,      # NODE_NAMESPACE
    'method': 8192,       # NODE_METHOD
    'function': 4096,     # NODE_FUNCTION
    'field': 2048,        # NODE_FIELD
    'property': 2048,     # NODE_FIELD
    'enum': 16384,        # NODE_ENUM
    'enum_member': 32768, # NODE_ENUM_CONSTANT
    'variable': 1024,     # NODE_GLOBAL_VARIABLE
    'type_alias': 65536,  # NODE_TYPEDEF
    'union': 1048576,     # NODE_UNION
    'file': 262144,       # NODE_FILE
}

EDGE_MAP = {
    'calls': 8,           # EDGE_CALL
    'dispatches_to': 8,   # EDGE_CALL
    'contains': 1,        # EDGE_MEMBER
    'defines': 1,         # EDGE_MEMBER
    'method': 1,          # EDGE_MEMBER
    'references': 4,      # EDGE_USAGE
    'instantiates': 4,    # EDGE_USAGE
    'uses': 4,            # EDGE_USAGE
    'extends': 16,        # EDGE_INHERITANCE
    'inherits': 16,       # EDGE_INHERITANCE
    'implements': 16,     # EDGE_INHERITANCE
    'imports': 512,       # EDGE_IMPORT
    'imports_from': 512,  # EDGE_IMPORT
    'overrides': 32,      # EDGE_OVERRIDE
}
DEFINITION_IMPLICIT = 1
DEFINITION_EXPLICIT = 2


def create_srctrl_schema(cursor):
    cursor.executescript('''
    PRAGMA synchronous = OFF;
    PRAGMA journal_mode = MEMORY;

    CREATE TABLE meta(id INTEGER, key TEXT, value TEXT, PRIMARY KEY(id));
    CREATE TABLE element(id INTEGER, PRIMARY KEY(id));
    CREATE TABLE element_component(id INTEGER, element_id INTEGER, type INTEGER, data TEXT, PRIMARY KEY(id), FOREIGN KEY(element_id) REFERENCES element(id) ON DELETE CASCADE);
    CREATE TABLE edge(id INTEGER NOT NULL, type INTEGER NOT NULL, source_node_id INTEGER NOT NULL, target_node_id INTEGER NOT NULL, PRIMARY KEY(id), FOREIGN KEY(id) REFERENCES element(id) ON DELETE CASCADE, FOREIGN KEY(source_node_id) REFERENCES node(id) ON DELETE CASCADE, FOREIGN KEY(target_node_id) REFERENCES node(id) ON DELETE CASCADE);
    CREATE TABLE node(id INTEGER NOT NULL, type INTEGER NOT NULL, serialized_name TEXT, PRIMARY KEY(id), FOREIGN KEY(id) REFERENCES element(id) ON DELETE CASCADE);
    CREATE TABLE symbol(id INTEGER NOT NULL, definition_kind INTEGER NOT NULL, PRIMARY KEY(id), FOREIGN KEY(id) REFERENCES node(id) ON DELETE CASCADE);
    CREATE TABLE file(id INTEGER NOT NULL, path TEXT, language TEXT, modification_time TEXT, indexed INTEGER, complete INTEGER, line_count INTEGER, PRIMARY KEY(id), FOREIGN KEY(id) REFERENCES node(id) ON DELETE CASCADE);
    CREATE TABLE filecontent(id INTEGER, content TEXT, PRIMARY KEY(id), FOREIGN KEY(id) REFERENCES file(id) ON DELETE CASCADE ON UPDATE CASCADE);
    CREATE TABLE local_symbol(id INTEGER NOT NULL, name TEXT, PRIMARY KEY(id), FOREIGN KEY(id) REFERENCES element(id) ON DELETE CASCADE);
    CREATE TABLE source_location(id INTEGER NOT NULL, file_node_id INTEGER, start_line INTEGER, start_column INTEGER, end_line INTEGER, end_column INTEGER, type INTEGER, PRIMARY KEY(id), FOREIGN KEY(file_node_id) REFERENCES node(id) ON DELETE CASCADE);
    CREATE TABLE occurrence(element_id INTEGER NOT NULL, source_location_id INTEGER NOT NULL, PRIMARY KEY(element_id, source_location_id), FOREIGN KEY(element_id) REFERENCES element(id) ON DELETE CASCADE, FOREIGN KEY(source_location_id) REFERENCES source_location(id) ON DELETE CASCADE);
    CREATE TABLE component_access(node_id INTEGER NOT NULL, type INTEGER NOT NULL, PRIMARY KEY(node_id), FOREIGN KEY(node_id) REFERENCES node(id) ON DELETE CASCADE);
    CREATE TABLE error(id INTEGER NOT NULL, message TEXT, fatal INTEGER NOT NULL, indexed INTEGER NOT NULL, translation_unit TEXT, PRIMARY KEY(id), FOREIGN KEY(id) REFERENCES element(id) ON DELETE CASCADE);
    ''')

    cursor.execute("INSERT INTO meta VALUES (1, 'project_settings', '<config></config>')")
    cursor.execute("INSERT INTO meta VALUES (2, 'storage_version', '25')")
    cursor.execute(f"INSERT INTO meta VALUES (3, 'timestamp', '{time.strftime('%Y-%m-%d %H:%M:%S')}')")

def create_srctrl_indexes(cursor):
    cursor.executescript('''
    CREATE INDEX edge_source_node_id_index ON edge(source_node_id);
    CREATE INDEX edge_target_node_id_index ON edge(target_node_id);
    CREATE INDEX node_serialized_name_index ON node(serialized_name);
    CREATE INDEX source_location_file_node_id_index ON source_location(file_node_id);
    CREATE INDEX occurrence_element_id_index ON occurrence(element_id);
    CREATE INDEX occurrence_source_location_id_index ON occurrence(source_location_id);
    CREATE INDEX element_component_foreign_key_index ON element_component(element_id);
    CREATE INDEX edge_source_foreign_key_index ON edge(source_node_id);
    CREATE INDEX edge_target_foreign_key_index ON edge(target_node_id);
    CREATE INDEX source_location_foreign_key_index ON source_location(file_node_id);
    CREATE INDEX occurrence_element_foreign_key_index ON occurrence(element_id);
    CREATE INDEX occurrence_source_location_foreign_key_index ON occurrence(source_location_id);
    ''')

def write_project_xml(prj_path, repo_root):
    content = f'''<?xml version="1.0" encoding="utf-8" ?>
<config>
    <source_groups>
        <source_group_codemap>
            <name>CodeMap Imported Group</name>
            <source_paths>
                <source_path>{repo_root}</source_path>
            </source_paths>
            <status>enabled</status>
            <type>Empty Source Group</type>
        </source_group_codemap>
    </source_groups>
    <version>8</version>
</config>
'''
    with open(prj_path, 'w', encoding='utf-8') as f:
        f.write(content)

def convert_codegraph_db(src_db_path, out_prj_path, out_db_path):
    print(f"Connecting to CodeGraph database: {src_db_path}")
    conn_src = sqlite3.connect(src_db_path)
    c_src = conn_src.cursor()

    # Sample file to determine repo root
    sample_files = [row[0] for row in c_src.execute("SELECT file_path FROM nodes WHERE file_path != '' LIMIT 5").fetchall()]
    repo_root = find_repo_root(src_db_path, sample_files)
    print(f"Detected repository root: {repo_root}")

    if os.path.exists(out_db_path):
        os.remove(out_db_path)

    conn_dst = sqlite3.connect(out_db_path)
    c_dst = conn_dst.cursor()
    create_srctrl_schema(c_dst)

    elem_id = 1
    node_id_map = {}
    file_node_map = {}

    file_node_ids = set(r[0] for r in c_src.execute("SELECT id FROM nodes WHERE kind = 'file'").fetchall())

    print("Fetching files...")
    files_data = c_src.execute("SELECT path, language, size FROM files").fetchall()
    if not files_data:
        files_data = [(row[0], 'unknown', 0) for row in c_src.execute("SELECT DISTINCT file_path FROM nodes WHERE file_path != ''").fetchall()]

    for fp, lang, size in files_data:
        if not fp:
            continue
        abs_path = os.path.normpath(os.path.join(repo_root, fp)).replace('\\', '/')
        fid = elem_id
        elem_id += 1
        file_node_map[fp] = fid

        lc = 0
        if os.path.exists(abs_path):
            try:
                with open(abs_path, 'r', encoding='utf-8', errors='ignore') as f:
                    lc = sum(1 for _ in f)
            except:
                pass

        c_dst.execute("INSERT INTO element VALUES (?)", (fid,))
        c_dst.execute("INSERT INTO node VALUES (?, ?, ?)", (fid, 262144, serialize_file_name(abs_path)))
        c_dst.execute("INSERT INTO file VALUES (?, ?, ?, ?, ?, ?, ?)",
                      (fid, abs_path, lang or 'csharp', '2026-10-11', 1, 1, lc))

    element_batch = []
    node_batch = []
    symbol_batch = []

    # Create top-level module nodes from project directories
    module_node_map = {}
    for fp in file_node_map.keys():
        if fp and '/' in fp:
            top_dir = fp.split('/')[0]
            if top_dir and not top_dir.startswith('.') and top_dir not in module_node_map:
                mid = elem_id
                elem_id += 1
                module_node_map[top_dir] = mid
                sname = serialize_name(top_dir, delimiter='.')
                element_batch.append((mid,))
                node_batch.append((mid, 8, sname))  # NODE_MODULE = 8
                symbol_batch.append((mid, DEFINITION_EXPLICIT))
    print(f"Created {len(module_node_map)} project module nodes.")
    print("Fetching nodes...")
    c_src.execute("SELECT id, kind, name, qualified_name, file_path, start_line, end_line, start_column, end_column, visibility FROM nodes")
    
    loc_id = 1
    access_batch = []
    loc_batch = []
    occ_batch = []
    module_child_pairs = set()
    for row in c_src.fetchall():
        str_id, kind, name, qname, fp, sline, eline, scol, ecol, vis = row
        if kind == 'file':
            node_id_map[str_id] = file_node_map.get(fp, elem_id)
            continue

        nid = elem_id
        elem_id += 1
        node_id_map[str_id] = nid

        ntype = KIND_MAP.get(kind.lower(), 1)
        is_method = (kind.lower() in ('method', 'function'))
        sname = serialize_name(qname or name, delimiter='.', is_method=is_method)

        element_batch.append((nid,))
        node_batch.append((nid, ntype, sname))
        symbol_batch.append((nid, DEFINITION_EXPLICIT))

        if vis:
            v = vis.lower()
            if 'public' in v:
                access_batch.append((nid, 1))
            elif 'protected' in v:
                access_batch.append((nid, 2))
            elif 'private' in v:
                access_batch.append((nid, 3))

        if fp and '/' in fp:
            top_dir = fp.split('/')[0]
            if top_dir in module_node_map and kind.lower() in ('namespace', 'class', 'struct', 'interface', 'function'):
                module_child_pairs.add((module_node_map[top_dir], nid))

        file_nid = file_node_map.get(fp)
        if file_nid and sline and eline:
            lid_tok = loc_id
            loc_id += 1
            loc_batch.append((lid_tok, file_nid, sline, scol or 1, sline, ecol or (scol or 1) + len(name), 0))
            occ_batch.append((nid, lid_tok))

            if eline > sline:
                lid_scope = loc_id
                loc_id += 1
                loc_batch.append((lid_scope, file_nid, sline, 1, eline, ecol or 1, 1))
                occ_batch.append((nid, lid_scope))

    print(f"Writing {len(node_batch)} nodes and {len(loc_batch)} source locations...")
    c_dst.executemany("INSERT INTO element VALUES (?)", element_batch)
    c_dst.executemany("INSERT INTO node VALUES (?, ?, ?)", node_batch)
    c_dst.executemany("INSERT INTO symbol VALUES (?, ?)", symbol_batch)
    c_dst.executemany("INSERT INTO component_access VALUES (?, ?)", access_batch)
    c_dst.executemany("INSERT INTO source_location VALUES (?, ?, ?, ?, ?, ?, ?)", loc_batch)
    c_dst.executemany("INSERT INTO occurrence VALUES (?, ?)", occ_batch)

    print("Fetching edges...")
    c_src.execute("SELECT source, target, kind FROM edges")
    edge_batch = []
    edge_elem_batch = []

    for source_id, target_id, ekind in c_src.fetchall():
        src_nid = node_id_map.get(source_id)
        tgt_nid = node_id_map.get(target_id)
        if src_nid and tgt_nid:
            eid = elem_id
            elem_id += 1
            if ekind.lower() == 'contains' and source_id in file_node_ids:
                # Map file containment to EDGE_USAGE (type 4), NOT EDGE_MEMBER,
                # so symbols do not become children of files in hierarchy cache!
                etype = 4
            else:
                etype = EDGE_MAP.get(ekind.lower(), 4)
            edge_elem_batch.append((eid,))
            edge_batch.append((eid, etype, src_nid, tgt_nid))

    # Add module containment edges (Module -> Namespace / Class)
    for mod_nid, child_nid in module_child_pairs:
        eid = elem_id
        elem_id += 1
        edge_elem_batch.append((eid,))
        edge_batch.append((eid, 1, mod_nid, child_nid))  # 1 = EDGE_MEMBER

    print(f"Writing {len(edge_batch)} edges...")
    c_dst.executemany("INSERT INTO element VALUES (?)", edge_elem_batch)
    c_dst.executemany("INSERT INTO edge VALUES (?, ?, ?, ?)", edge_batch)

    print("Building indexes...")
    create_srctrl_indexes(c_dst)

    conn_dst.commit()
    conn_dst.close()
    conn_src.close()

    write_project_xml(out_prj_path, repo_root)
    print(f"Conversion complete! Created {out_prj_path} and {out_db_path}")
    return out_prj_path

def convert_graphify_json(src_json_path, out_prj_path, out_db_path):
    print(f"Loading Graphify JSON: {src_json_path}")
    with open(src_json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    sample_files = [n.get('source_file') for n in data.get('nodes', [])[:10] if n.get('source_file')]
    repo_root = find_repo_root(src_json_path, sample_files)
    print(f"Detected repository root: {repo_root}")

    if os.path.exists(out_db_path):
        os.remove(out_db_path)

    conn_dst = sqlite3.connect(out_db_path)
    c_dst = conn_dst.cursor()
    create_srctrl_schema(c_dst)

    elem_id = 1
    file_node_map = {}
    node_id_map = {}

    def parse_line(loc_str):
        if not loc_str:
            return 1
        m = re.search(r'\d+', str(loc_str))
        return int(m.group(0)) if m else 1

    # 1. Register files
    for n in data.get('nodes', []):
        sf = n.get('source_file')
        if sf and sf not in file_node_map:
            abs_path = os.path.normpath(os.path.join(repo_root, sf)).replace('\\', '/')
            fid = elem_id
            elem_id += 1
            file_node_map[sf] = fid

            lc = 0
            if os.path.exists(abs_path):
                try:
                    with open(abs_path, 'r', encoding='utf-8', errors='ignore') as f:
                        lc = sum(1 for _ in f)
                except:
                    pass

            c_dst.execute("INSERT INTO element VALUES (?)", (fid,))
            c_dst.execute("INSERT INTO node VALUES (?, ?, ?)", (fid, 262144, serialize_file_name(abs_path)))
            c_dst.execute("INSERT INTO file VALUES (?, ?, ?, ?, ?, ?, ?)",
                          (fid, abs_path, 'csharp', '2026-10-11', 1, 1, lc))
    element_batch = []
    node_batch = []
    symbol_batch = []

    # Create top-level module nodes from source file paths
    module_node_map = {}
    for sf in file_node_map.keys():
        if sf and '/' in sf:
            top_dir = sf.split('/')[0]
            if top_dir and not top_dir.startswith('.') and top_dir not in module_node_map:
                mid = elem_id
                elem_id += 1
                module_node_map[top_dir] = mid
                sname = serialize_name(top_dir, delimiter='.')
                element_batch.append((mid,))
                node_batch.append((mid, 8, sname))  # NODE_MODULE = 8
                symbol_batch.append((mid, DEFINITION_EXPLICIT))
    print(f"Created {len(module_node_map)} project module nodes in graphify.")

    # 2. Register nodes
    loc_id = 1
    loc_batch = []
    occ_batch = []
    module_child_pairs = set()
    for n in data.get('nodes', []):
        nid = elem_id
        elem_id += 1
        node_id_map[n['id']] = nid

        is_class = n.get('_callable_class', False)
        is_callable = n.get('_callable', False) and not is_class
        ntype = 128 if is_class else (8192 if is_callable else 1)

        meta = n.get('metadata', {})
        ns = meta.get('namespace') if isinstance(meta, dict) else None
        sname = serialize_name(n.get('label', n['id']), ns=ns, is_method=is_callable)

        element_batch.append((nid,))
        node_batch.append((nid, ntype, sname))
        symbol_batch.append((nid, DEFINITION_EXPLICIT))

        sf = n.get('source_file')
        file_nid = file_node_map.get(sf)
        sloc = n.get('source_location')
        if file_nid:
            line = parse_line(sloc)
            lid = loc_id
            loc_id += 1
            loc_batch.append((lid, file_nid, line, 1, line, 10, 0))
            occ_batch.append((nid, lid))

        sf = n.get('source_file')
        if sf and '/' in sf:
            top_dir = sf.split('/')[0]
            if top_dir in module_node_map and (is_class or not is_callable):
                module_child_pairs.add((module_node_map[top_dir], nid))

    print(f"Writing {len(node_batch)} nodes and {len(loc_batch)} source locations...")
    c_dst.executemany("INSERT INTO element VALUES (?)", element_batch)
    c_dst.executemany("INSERT INTO node VALUES (?, ?, ?)", node_batch)
    c_dst.executemany("INSERT INTO symbol VALUES (?, ?)", symbol_batch)
    c_dst.executemany("INSERT INTO source_location VALUES (?, ?, ?, ?, ?, ?, ?)", loc_batch)
    c_dst.executemany("INSERT INTO occurrence VALUES (?, ?)", occ_batch)

    # 3. Register links
    edge_batch = []
    edge_elem_batch = []
    for link in data.get('links', []):
        src = link.get('source')
        tgt = link.get('target')
        src_nid = node_id_map.get(src)
        tgt_nid = node_id_map.get(tgt)
        if src_nid and tgt_nid:
            rel = link.get('relation', 'references')
            etype = EDGE_MAP.get(rel.lower(), 4)
            eid = elem_id
            elem_id += 1
            edge_elem_batch.append((eid,))
            edge_batch.append((eid, etype, src_nid, tgt_nid))


    # Add module containment edges (Module -> Class)
    for mod_nid, child_nid in module_child_pairs:
        eid = elem_id
        elem_id += 1
        edge_elem_batch.append((eid,))
        edge_batch.append((eid, 1, mod_nid, child_nid))  # 1 = EDGE_MEMBER

    print(f"Writing {len(edge_batch)} edges...")
    c_dst.executemany("INSERT INTO element VALUES (?)", edge_elem_batch)
    c_dst.executemany("INSERT INTO edge VALUES (?, ?, ?, ?)", edge_batch)

    print("Building indexes...")
    create_srctrl_indexes(c_dst)

    conn_dst.commit()
    conn_dst.close()

    write_project_xml(out_prj_path, repo_root)
    print(f"Conversion complete! Created {out_prj_path} and {out_db_path}")
    return out_prj_path

def import_codemap(target_path):
    target = os.path.abspath(target_path)
    if os.path.isdir(target):
        # Look for .codegraph/codegraph.db or codegraph.db
        candidate_db = os.path.join(target, '.codegraph', 'codegraph.db')
        if os.path.exists(candidate_db):
            target = candidate_db
        elif os.path.exists(os.path.join(target, 'codegraph.db')):
            target = os.path.join(target, 'codegraph.db')
        elif os.path.exists(os.path.join(target, 'graphify-out', 'graph.json')):
            target = os.path.join(target, 'graphify-out', 'graph.json')
        elif os.path.exists(os.path.join(target, 'graph.json')):
            target = os.path.join(target, 'graph.json')

    target_dir = os.path.dirname(target)
    base_name, ext = os.path.splitext(os.path.basename(target))

    prj_path = os.path.join(target_dir, base_name + '.srctrlprj')
    db_path = os.path.join(target_dir, base_name + '.srctrldb')

    # Cache check
    if os.path.exists(prj_path) and os.path.exists(db_path):
        src_mtime = os.path.getmtime(target)
        db_mtime = os.path.getmtime(db_path)
        if db_mtime >= src_mtime:
            print(f"Using up-to-date cached project: {prj_path}")
            return prj_path

    if ext.lower() == '.json':
        return convert_graphify_json(target, prj_path, db_path)
    else:
        return convert_codegraph_db(target, prj_path, db_path)

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: import_codemap.py <path-to-codegraph.db|graph.json|directory>")
        sys.exit(1)

    res = import_codemap(sys.argv[1])
    print(f"PROJECT_PATH:{res}")
