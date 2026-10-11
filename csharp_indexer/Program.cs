using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Linq;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.Data.Sqlite;

namespace SourcetrailCsharpIndexer
{
    class Program
    {
        // Sourcetrail Node Kinds (from NodeKind.h)
        const int NODE_SYMBOL = 1 << 0;
        const int NODE_TYPE = 1 << 1;
        const int NODE_BUILTIN_TYPE = 1 << 2;
        const int NODE_MODULE = 1 << 3;
        const int NODE_NAMESPACE = 1 << 4;
        const int NODE_PACKAGE = 1 << 5;
        const int NODE_STRUCT = 1 << 6;
        const int NODE_CLASS = 1 << 7;
        const int NODE_INTERFACE = 1 << 8;
        const int NODE_ANNOTATION = 1 << 9;
        const int NODE_GLOBAL_VARIABLE = 1 << 10;
        const int NODE_FIELD = 1 << 11;
        const int NODE_FUNCTION = 1 << 12;
        const int NODE_METHOD = 1 << 13;
        const int NODE_ENUM = 1 << 14;
        const int NODE_ENUM_CONSTANT = 1 << 15;
        const int NODE_TYPEDEF = 1 << 16;
        const int NODE_TYPE_PARAMETER = 1 << 17;
        const int NODE_FILE = 1 << 18;
        const int NODE_MACRO = 1 << 19;
        const int NODE_UNION = 1 << 20;

        // Sourcetrail Edge Types (from Edge.h)
        const int EDGE_UNDEFINED = 0;
        const int EDGE_MEMBER = 1 << 0;                  // 1
        const int EDGE_TYPE_USAGE = 1 << 1;              // 2
        const int EDGE_USAGE = 1 << 2;                   // 4
        const int EDGE_CALL = 1 << 3;                    // 8
        const int EDGE_INHERITANCE = 1 << 4;             // 16
        const int EDGE_OVERRIDE = 1 << 5;                // 32
        const int EDGE_TYPE_ARGUMENT = 1 << 6;           // 64
        const int EDGE_TEMPLATE_SPECIALIZATION = 1 << 7; // 128
        const int EDGE_INCLUDE = 1 << 8;                 // 256
        const int EDGE_IMPORT = 1 << 9;                  // 512

        // Sourcetrail Definition Kinds (from DefinitionKind.h)
        const int DEFINITION_NONE = 0;
        const int DEFINITION_IMPLICIT = 1;
        const int DEFINITION_EXPLICIT = 2;

        // Location Types (from LocationType.h)
        const int LOCATION_TOKEN = 0;
        const int LOCATION_SCOPE = 1;
        const int LOCATION_QUALIFIER = 2;
        const int LOCATION_SIGNATURE = 3;

        static int Main(string[] args)
        {
            if (args.Length == 0 || args[0] == "--help" || args[0] == "-h")
            {
                Console.WriteLine("SourcetrailCsharpIndexer - C# source code indexer using Roslyn and csharp-ls");
                Console.WriteLine("Usage: SourcetrailCsharpIndexer index --source-file-path <file> --database-file-path <db>");
                Console.WriteLine("       SourcetrailCsharpIndexer check-environment");
                return 0;
            }

            if (args[0] == "check-environment")
            {
                string userProfile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
                string csharpLsPath = Path.Combine(userProfile, ".dotnet", "tools", "csharp-ls.exe");
                bool hasCsharpLs = File.Exists(csharpLsPath);
                Console.WriteLine("The provided path is a valid C# environment (Roslyn backend, csharp-ls: " + (hasCsharpLs ? "available" : "not found") + ").");
                return 0;
            }

            if (args[0] != "index")
            {
                Console.Error.WriteLine("Unknown command: " + args[0]);
                return 1;
            }

            string sourceFilePath = null;
            string databaseFilePath = null;
            bool isVerbose = false;
            bool clearDb = false;

            for (int i = 1; i < args.Length; i++)
            {
                if (args[i] == "--source-file-path" && i + 1 < args.Length)
                    sourceFilePath = args[++i];
                else if (args[i] == "--database-file-path" && i + 1 < args.Length)
                    databaseFilePath = args[++i];
                else if (args[i] == "--verbose")
                    isVerbose = true;
                else if (args[i] == "--clear")
                    clearDb = true;
            }

            if (string.IsNullOrEmpty(sourceFilePath) || string.IsNullOrEmpty(databaseFilePath))
            {
                Console.Error.WriteLine("Missing required arguments: --source-file-path or --database-file-path");
                return 1;
            }

            try
            {
                IndexFile(sourceFilePath, databaseFilePath, isVerbose, clearDb);
                if (isVerbose)
                {
                    Console.WriteLine("Finished indexing " + sourceFilePath);
                }
                return 0;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine("ERROR: Exception during indexing: " + ex.Message);
                return -1;
            }
        }

        static void IndexFile(string sourceFilePath, string databaseFilePath, bool isVerbose, bool clearDb)
        {
            sourceFilePath = Path.GetFullPath(sourceFilePath).Replace('\\', '/');
            if (!File.Exists(sourceFilePath))
            {
                throw new FileNotFoundException("Source file not found: " + sourceFilePath);
            }

            string code = File.ReadAllText(sourceFilePath, new UTF8Encoding(false, false));
            var tree = CSharpSyntaxTree.ParseText(code, path: sourceFilePath);
            var root = tree.GetCompilationUnitRoot();
            int lineCount = code.Split('\n').Length;

            using var connection = new SqliteConnection($"Data Source={databaseFilePath}");
            connection.Open();

            using (var cmd = connection.CreateCommand())
            {
                cmd.CommandText = @"
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
                ";
                cmd.ExecuteNonQuery();
            }

            if (clearDb)
            {
                using var clearCmd = connection.CreateCommand();
                clearCmd.CommandText = "DELETE FROM occurrence; DELETE FROM source_location; DELETE FROM edge; DELETE FROM symbol; DELETE FROM file; DELETE FROM filecontent; DELETE FROM node; DELETE FROM element;";
                clearCmd.ExecuteNonQuery();
            }

            using var transaction = connection.BeginTransaction();

            int nextElementId = 1;
            using (var cmd = connection.CreateCommand())
            {
                cmd.Transaction = transaction;
                cmd.CommandText = "SELECT COALESCE(MAX(id), 0) FROM element;";
                nextElementId = Convert.ToInt32(cmd.ExecuteScalar()) + 1;
            }

            int nextLocationId = 1;
            using (var cmd = connection.CreateCommand())
            {
                cmd.Transaction = transaction;
                cmd.CommandText = "SELECT COALESCE(MAX(id), 0) FROM source_location;";
                nextLocationId = Convert.ToInt32(cmd.ExecuteScalar()) + 1;
            }

            // Write metadata
            using (var cmd = connection.CreateCommand())
            {
                cmd.Transaction = transaction;
                cmd.CommandText = "INSERT OR IGNORE INTO meta(id, key, value) VALUES (1, 'db_version', '25'), (2, 'app_version', '2021.4.19');";
                cmd.ExecuteNonQuery();
            }

            // Record File Node: serialized name format is "/\tm<filePath>\ts\tp"
            int fileNodeId = nextElementId++;
            using (var cmd = connection.CreateCommand())
            {
                cmd.Transaction = transaction;
                cmd.CommandText = "INSERT INTO element(id) VALUES (@id); INSERT INTO node(id, type, serialized_name) VALUES (@id, @type, @name);";
                cmd.Parameters.AddWithValue("@id", fileNodeId);
                cmd.Parameters.AddWithValue("@type", NODE_FILE);
                cmd.Parameters.AddWithValue("@name", "/\tm" + sourceFilePath + "\ts\tp");
                cmd.ExecuteNonQuery();

                cmd.CommandText = "INSERT OR REPLACE INTO file(id, path, language, modification_time, indexed, complete, line_count) VALUES (@id, @path, 'csharp', @mod, 1, 1, @lines);";
                cmd.Parameters.Clear();
                cmd.Parameters.AddWithValue("@id", fileNodeId);
                cmd.Parameters.AddWithValue("@path", sourceFilePath);
                cmd.Parameters.AddWithValue("@mod", File.GetLastWriteTime(sourceFilePath).ToString("yyyy-MM-dd HH:mm:ss"));
                cmd.Parameters.AddWithValue("@lines", lineCount);
                cmd.ExecuteNonQuery();

                cmd.CommandText = "INSERT OR REPLACE INTO filecontent(id, content) VALUES (@id, @content);";
                cmd.Parameters.Clear();
                cmd.Parameters.AddWithValue("@id", fileNodeId);
                cmd.Parameters.AddWithValue("@content", code);
                cmd.ExecuteNonQuery();
            }

            var symbolMap = new Dictionary<string, int>();
            var edgeSet = new HashSet<(int, int, int)>();

            string FormatSerializedName(string fullName, string postfix = "")
            {
                if (fullName.StartsWith("/\tm") || (fullName.StartsWith("<") && fullName.EndsWith(">")))
                    return fullName;

                string[] parts = fullName.Split('.');
                var sb = new System.Text.StringBuilder(".\tm");
                for (int i = 0; i < parts.Length; i++)
                {
                    if (i > 0) sb.Append("\tn");
                    sb.Append(parts[i]);
                    sb.Append("\ts");
                    if (i == parts.Length - 1)
                    {
                        sb.Append("\tp");
                        sb.Append(postfix);
                    }
                    else
                    {
                        sb.Append("\tp");
                    }
                }
                return sb.ToString();
            }

            int RecordNode(string nameKey, int nodeType, int definitionKind = DEFINITION_EXPLICIT, string postfix = "")
            {
                string key = $"{nameKey}{postfix}";
                if (symbolMap.TryGetValue(key, out int existingId))
                {
                    if (definitionKind == DEFINITION_EXPLICIT)
                    {
                        using var updateCmd = connection.CreateCommand();
                        updateCmd.Transaction = transaction;
                        updateCmd.CommandText = "UPDATE symbol SET definition_kind = 2 WHERE id = @id AND definition_kind < 2;";
                        updateCmd.Parameters.AddWithValue("@id", existingId);
                        updateCmd.ExecuteNonQuery();
                    }
                    return existingId;
                }

                string serializedName = FormatSerializedName(nameKey, postfix);
                int id = nextElementId++;
                using var cmd = connection.CreateCommand();
                cmd.Transaction = transaction;
                cmd.CommandText = "INSERT INTO element(id) VALUES (@id); INSERT INTO node(id, type, serialized_name) VALUES (@id, @type, @name); INSERT INTO symbol(id, definition_kind) VALUES (@id, @defKind);";
                cmd.Parameters.AddWithValue("@id", id);
                cmd.Parameters.AddWithValue("@type", nodeType);
                cmd.Parameters.AddWithValue("@name", serializedName);
                cmd.Parameters.AddWithValue("@defKind", definitionKind);
                cmd.ExecuteNonQuery();

                symbolMap[key] = id;
                return id;
            }

            void RecordLocation(int elementId, FileLinePositionSpan span, int locType)
            {
                int locId = nextLocationId++;
                using var cmd = connection.CreateCommand();
                cmd.Transaction = transaction;
                cmd.CommandText = "INSERT INTO source_location(id, file_node_id, start_line, start_column, end_line, end_column, type) VALUES (@id, @fileId, @sLine, @sCol, @eLine, @eCol, @type); INSERT INTO occurrence(element_id, source_location_id) VALUES (@elemId, @id);";
                cmd.Parameters.AddWithValue("@id", locId);
                cmd.Parameters.AddWithValue("@fileId", fileNodeId);
                cmd.Parameters.AddWithValue("@sLine", span.StartLinePosition.Line + 1);
                cmd.Parameters.AddWithValue("@sCol", span.StartLinePosition.Character + 1);
                cmd.Parameters.AddWithValue("@eLine", span.EndLinePosition.Line + 1);
                cmd.Parameters.AddWithValue("@eCol", span.EndLinePosition.Character + 1);
                cmd.Parameters.AddWithValue("@type", locType);
                cmd.Parameters.AddWithValue("@elemId", elementId);
                cmd.ExecuteNonQuery();
            }

            int RecordEdge(int sourceId, int targetId, int edgeType)
            {
                if (sourceId <= 0 || targetId <= 0 || sourceId == targetId)
                    return 0;

                var key = (sourceId, targetId, edgeType);
                if (edgeSet.Contains(key))
                    return 0;
                edgeSet.Add(key);

                int edgeId = nextElementId++;
                using var cmd = connection.CreateCommand();
                cmd.Transaction = transaction;
                cmd.CommandText = "INSERT INTO element(id) VALUES (@id); INSERT INTO edge(id, type, source_node_id, target_node_id) VALUES (@id, @type, @src, @tgt);";
                cmd.Parameters.AddWithValue("@id", edgeId);
                cmd.Parameters.AddWithValue("@type", edgeType);
                cmd.Parameters.AddWithValue("@src", sourceId);
                cmd.Parameters.AddWithValue("@tgt", targetId);
                cmd.ExecuteNonQuery();
                return edgeId;
            }

            // AST Walker threading parentSymbolId
            void Walk(SyntaxNode node, string currentNamespace, string currentType, int parentSymbolId)
            {
                if (node is BaseNamespaceDeclarationSyntax nsDecl)
                {
                    string nsName = nsDecl.Name.ToString();
                    string fullNs = string.IsNullOrEmpty(currentNamespace) ? nsName : $"{currentNamespace}.{nsName}";

                    string[] nsParts = nsName.Split('.');
                    string builtNs = currentNamespace;
                    int prevNsId = parentSymbolId;
                    for (int i = 0; i < nsParts.Length; i++)
                    {
                        builtNs = string.IsNullOrEmpty(builtNs) ? nsParts[i] : $"{builtNs}.{nsParts[i]}";
                        int curNsId = RecordNode(builtNs, NODE_NAMESPACE, DEFINITION_EXPLICIT);
                        if (prevNsId > 0)
                        {
                            RecordEdge(prevNsId, curNsId, EDGE_MEMBER);
                        }
                        prevNsId = curNsId;
                    }
                    int nsId = prevNsId;
                    RecordLocation(nsId, nsDecl.Name.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    RecordLocation(nsId, nsDecl.GetLocation().GetLineSpan(), LOCATION_SCOPE);

                    foreach (var child in nsDecl.ChildNodes())
                    {
                        Walk(child, fullNs, currentType, nsId);
                    }
                    return;
                }
                else if (node is TypeDeclarationSyntax typeDecl)
                {
                    string typeName = typeDecl.Identifier.Text;
                    string fullTypeName = string.IsNullOrEmpty(currentType)
                        ? (string.IsNullOrEmpty(currentNamespace) ? typeName : $"{currentNamespace}.{typeName}")
                        : $"{currentType}.{typeName}";

                    int nodeKind = NODE_CLASS;
                    if (typeDecl is InterfaceDeclarationSyntax) nodeKind = NODE_INTERFACE;
                    else if (typeDecl is StructDeclarationSyntax) nodeKind = NODE_STRUCT;
                    else if (typeDecl is RecordDeclarationSyntax) nodeKind = NODE_CLASS;

                    int typeId = RecordNode(fullTypeName, nodeKind, DEFINITION_EXPLICIT);
                    RecordLocation(typeId, typeDecl.Identifier.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    RecordLocation(typeId, typeDecl.GetLocation().GetLineSpan(), LOCATION_SCOPE);

                    if (parentSymbolId > 0)
                    {
                        RecordEdge(parentSymbolId, typeId, EDGE_MEMBER);
                    }

                    if (typeDecl.BaseList != null)
                    {
                        foreach (var baseType in typeDecl.BaseList.Types)
                        {
                            string baseTypeName = baseType.Type.ToString();
                            int targetBaseId = RecordNode(baseTypeName, NODE_TYPE, DEFINITION_NONE);
                            int edgeId = RecordEdge(typeId, targetBaseId, EDGE_INHERITANCE);
                            if (edgeId > 0)
                            {
                                RecordLocation(edgeId, baseType.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                            }
                        }
                    }

                    foreach (var child in typeDecl.ChildNodes())
                    {
                        Walk(child, currentNamespace, fullTypeName, typeId);
                    }
                    return;
                }
                else if (node is EnumDeclarationSyntax enumDecl)
                {
                    string enumName = enumDecl.Identifier.Text;
                    string fullEnumName = string.IsNullOrEmpty(currentType)
                        ? (string.IsNullOrEmpty(currentNamespace) ? enumName : $"{currentNamespace}.{enumName}")
                        : $"{currentType}.{enumName}";

                    int enumId = RecordNode(fullEnumName, NODE_ENUM, DEFINITION_EXPLICIT);
                    RecordLocation(enumId, enumDecl.Identifier.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    RecordLocation(enumId, enumDecl.GetLocation().GetLineSpan(), LOCATION_SCOPE);

                    if (parentSymbolId > 0)
                    {
                        RecordEdge(parentSymbolId, enumId, EDGE_MEMBER);
                    }

                    foreach (var member in enumDecl.Members)
                    {
                        string memberName = $"{fullEnumName}.{member.Identifier.Text}";
                        int memberId = RecordNode(memberName, NODE_ENUM_CONSTANT, DEFINITION_EXPLICIT);
                        RecordLocation(memberId, member.Identifier.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                        RecordEdge(enumId, memberId, EDGE_MEMBER);
                    }
                    return;
                }
                else if (node is MethodDeclarationSyntax methodDecl)
                {
                    string methodName = $"{currentType}.{methodDecl.Identifier.Text}";
                    int methodId = RecordNode(methodName, NODE_METHOD, DEFINITION_EXPLICIT, "()");
                    RecordLocation(methodId, methodDecl.Identifier.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    RecordLocation(methodId, methodDecl.GetLocation().GetLineSpan(), LOCATION_SCOPE);

                    if (parentSymbolId > 0)
                    {
                        RecordEdge(parentSymbolId, methodId, EDGE_MEMBER);
                    }

                    string retType = methodDecl.ReturnType.ToString();
                    if (retType != "void")
                    {
                        int retTypeId = RecordNode(retType, NODE_TYPE, DEFINITION_NONE);
                        int edgeId = RecordEdge(methodId, retTypeId, EDGE_TYPE_USAGE);
                        if (edgeId > 0)
                        {
                            RecordLocation(edgeId, methodDecl.ReturnType.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                        }
                    }

                    foreach (var child in methodDecl.ChildNodes())
                    {
                        Walk(child, currentNamespace, currentType, methodId);
                    }
                    return;
                }
                else if (node is ConstructorDeclarationSyntax ctorDecl)
                {
                    string ctorName = $"{currentType}.{ctorDecl.Identifier.Text}";
                    int ctorId = RecordNode(ctorName, NODE_METHOD, DEFINITION_EXPLICIT, "()");
                    RecordLocation(ctorId, ctorDecl.Identifier.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    RecordLocation(ctorId, ctorDecl.GetLocation().GetLineSpan(), LOCATION_SCOPE);

                    if (parentSymbolId > 0)
                    {
                        RecordEdge(parentSymbolId, ctorId, EDGE_MEMBER);
                    }

                    foreach (var child in ctorDecl.ChildNodes())
                    {
                        Walk(child, currentNamespace, currentType, ctorId);
                    }
                    return;
                }
                else if (node is PropertyDeclarationSyntax propDecl)
                {
                    string propName = $"{currentType}.{propDecl.Identifier.Text}";
                    int propId = RecordNode(propName, NODE_FIELD, DEFINITION_EXPLICIT);
                    RecordLocation(propId, propDecl.Identifier.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    RecordLocation(propId, propDecl.GetLocation().GetLineSpan(), LOCATION_SCOPE);

                    if (parentSymbolId > 0)
                    {
                        RecordEdge(parentSymbolId, propId, EDGE_MEMBER);
                    }

                    string pType = propDecl.Type.ToString();
                    int pTypeId = RecordNode(pType, NODE_TYPE, DEFINITION_NONE);
                    int edgeId = RecordEdge(propId, pTypeId, EDGE_TYPE_USAGE);
                    if (edgeId > 0)
                    {
                        RecordLocation(edgeId, propDecl.Type.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    }

                    foreach (var child in propDecl.ChildNodes())
                    {
                        Walk(child, currentNamespace, currentType, propId);
                    }
                    return;
                }
                else if (node is FieldDeclarationSyntax fieldDecl)
                {
                    string fType = fieldDecl.Declaration.Type.ToString();
                    int fTypeId = RecordNode(fType, NODE_TYPE, DEFINITION_NONE);

                    foreach (var variable in fieldDecl.Declaration.Variables)
                    {
                        string fieldName = $"{currentType}.{variable.Identifier.Text}";
                        int fieldId = RecordNode(fieldName, NODE_FIELD, DEFINITION_EXPLICIT);
                        RecordLocation(fieldId, variable.Identifier.GetLocation().GetLineSpan(), LOCATION_TOKEN);

                        if (parentSymbolId > 0)
                        {
                            RecordEdge(parentSymbolId, fieldId, EDGE_MEMBER);
                        }

                        int edgeId = RecordEdge(fieldId, fTypeId, EDGE_TYPE_USAGE);
                        if (edgeId > 0)
                        {
                            RecordLocation(edgeId, fieldDecl.Declaration.Type.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                        }
                    }
                    return;
                }
                else if (node is InvocationExpressionSyntax invocation)
                {
                    string calledName = invocation.Expression.ToString();
                    if (parentSymbolId > 0)
                    {
                        int calleeId = RecordNode(calledName, NODE_FUNCTION, DEFINITION_NONE, "()");
                        int callEdgeId = RecordEdge(parentSymbolId, calleeId, EDGE_CALL);
                        if (callEdgeId > 0)
                        {
                            RecordLocation(callEdgeId, invocation.Expression.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                        }
                    }

                    foreach (var child in invocation.ChildNodes())
                    {
                        Walk(child, currentNamespace, currentType, parentSymbolId);
                    }
                    return;
                }

                foreach (var child in node.ChildNodes())
                {
                    Walk(child, currentNamespace, currentType, parentSymbolId);
                }
            }

            Walk(root, "", "", 0);

            transaction.Commit();
        }
    }
}
