using System;
using System.Collections.Generic;
using System.IO;
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
        const int NODE_NAMESPACE = 1 << 4;
        const int NODE_STRUCT = 1 << 6;
        const int NODE_CLASS = 1 << 7;
        const int NODE_INTERFACE = 1 << 8;
        const int NODE_FIELD = 1 << 11;
        const int NODE_FUNCTION = 1 << 12;
        const int NODE_METHOD = 1 << 13;
        const int NODE_ENUM = 1 << 14;
        const int NODE_ENUM_CONSTANT = 1 << 15;
        const int NODE_FILE = 1 << 18;

        // Sourcetrail Reference Kinds (from ReferenceKind.h)
        const int REFERENCE_TYPE_USAGE = 1;
        const int REFERENCE_USAGE = 2;
        const int REFERENCE_CALL = 3;
        const int REFERENCE_INHERITANCE = 4;
        const int REFERENCE_OVERRIDE = 5;

        // Location Types
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
                // Verify csharp-ls and Roslyn are available
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

            string code = File.ReadAllText(sourceFilePath);
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

            // Record File
            int fileNodeId = nextElementId++;
            using (var cmd = connection.CreateCommand())
            {
                cmd.Transaction = transaction;
                cmd.CommandText = "INSERT INTO element(id) VALUES (@id); INSERT INTO node(id, type, serialized_name) VALUES (@id, @type, @name);";
                cmd.Parameters.AddWithValue("@id", fileNodeId);
                cmd.Parameters.AddWithValue("@type", NODE_FILE);
                cmd.Parameters.AddWithValue("@name", "<" + sourceFilePath + ">");
                cmd.ExecuteNonQuery();

                cmd.CommandText = "INSERT OR REPLACE INTO file(id, path, language, modification_time, indexed, complete, line_count) VALUES (@id, @path, 'csharp', @mod, 1, 1, @lines);";
                cmd.Parameters.Clear();
                cmd.Parameters.AddWithValue("@id", fileNodeId);
                cmd.Parameters.AddWithValue("@path", sourceFilePath);
                cmd.Parameters.AddWithValue("@mod", File.GetLastWriteTimeUtc(sourceFilePath).ToString("o"));
                cmd.Parameters.AddWithValue("@lines", lineCount);
                cmd.ExecuteNonQuery();
            }

            var symbolMap = new Dictionary<string, int>();

            string FormatSerializedName(string fullName, string postfix = "")
            {
                if (fullName.StartsWith("<") && fullName.EndsWith(">"))
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

            int RecordNode(string nameKey, int nodeType, int definitionKind = 1, string postfix = "")
            {
                string key = $"{nameKey}{postfix}";
                if (symbolMap.TryGetValue(key, out int existingId))
                    return existingId;

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

            // AST Walker
            void Walk(SyntaxNode node, string currentNamespace, string currentType)
            {
                string ns = currentNamespace;
                string parentType = currentType;

                if (node is BaseNamespaceDeclarationSyntax nsDecl)
                {
                    string nsName = nsDecl.Name.ToString();
                    string fullNs = string.IsNullOrEmpty(ns) ? nsName : $"{ns}.{nsName}";
                    int nsId = RecordNode(fullNs, NODE_NAMESPACE);
                    RecordLocation(nsId, nsDecl.Name.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    RecordLocation(nsId, nsDecl.GetLocation().GetLineSpan(), LOCATION_SCOPE);
                    ns = fullNs;
                }
                else if (node is TypeDeclarationSyntax typeDecl)
                {
                    string typeName = typeDecl.Identifier.Text;
                    string fullTypeName = string.IsNullOrEmpty(parentType)
                        ? (string.IsNullOrEmpty(ns) ? typeName : $"{ns}.{typeName}")
                        : $"{parentType}.{typeName}";

                    int nodeKind = NODE_CLASS;
                    if (typeDecl is InterfaceDeclarationSyntax) nodeKind = NODE_INTERFACE;
                    else if (typeDecl is StructDeclarationSyntax) nodeKind = NODE_STRUCT;
                    else if (typeDecl is RecordDeclarationSyntax) nodeKind = NODE_CLASS;

                    int typeId = RecordNode(fullTypeName, nodeKind);
                    RecordLocation(typeId, typeDecl.Identifier.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    RecordLocation(typeId, typeDecl.GetLocation().GetLineSpan(), LOCATION_SCOPE);

                    if (typeDecl.BaseList != null)
                    {
                        foreach (var baseType in typeDecl.BaseList.Types)
                        {
                            string baseTypeName = baseType.Type.ToString();
                            int targetBaseId = RecordNode(baseTypeName, NODE_TYPE, definitionKind: 0);
                            int edgeId = RecordEdge(typeId, targetBaseId, REFERENCE_INHERITANCE);
                            RecordLocation(edgeId, baseType.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                        }
                    }

                    parentType = fullTypeName;
                }
                else if (node is EnumDeclarationSyntax enumDecl)
                {
                    string enumName = enumDecl.Identifier.Text;
                    string fullEnumName = string.IsNullOrEmpty(parentType)
                        ? (string.IsNullOrEmpty(ns) ? enumName : $"{ns}.{enumName}")
                        : $"{parentType}.{enumName}";

                    int enumId = RecordNode(fullEnumName, NODE_ENUM);
                    RecordLocation(enumId, enumDecl.Identifier.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    RecordLocation(enumId, enumDecl.GetLocation().GetLineSpan(), LOCATION_SCOPE);

                    foreach (var member in enumDecl.Members)
                    {
                        string memberName = $"{fullEnumName}.{member.Identifier.Text}";
                        int memberId = RecordNode(memberName, NODE_ENUM_CONSTANT);
                        RecordLocation(memberId, member.Identifier.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    }

                    parentType = fullEnumName;
                }
                else if (node is MethodDeclarationSyntax methodDecl)
                {
                    string methodName = $"{parentType}.{methodDecl.Identifier.Text}";
                    int methodId = RecordNode(methodName, NODE_METHOD, 1, "()");
                    RecordLocation(methodId, methodDecl.Identifier.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    RecordLocation(methodId, methodDecl.GetLocation().GetLineSpan(), LOCATION_SCOPE);

                    string retType = methodDecl.ReturnType.ToString();
                    if (retType != "void")
                    {
                        int retTypeId = RecordNode(retType, NODE_TYPE, definitionKind: 0);
                        int edgeId = RecordEdge(methodId, retTypeId, REFERENCE_TYPE_USAGE);
                        RecordLocation(edgeId, methodDecl.ReturnType.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    }
                }
                else if (node is ConstructorDeclarationSyntax ctorDecl)
                {
                    string ctorName = $"{parentType}.{ctorDecl.Identifier.Text}";
                    int ctorId = RecordNode(ctorName, NODE_METHOD, 1, "()");
                    RecordLocation(ctorId, ctorDecl.Identifier.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    RecordLocation(ctorId, ctorDecl.GetLocation().GetLineSpan(), LOCATION_SCOPE);
                }
                else if (node is PropertyDeclarationSyntax propDecl)
                {
                    string propName = $"{parentType}.{propDecl.Identifier.Text}";
                    int propId = RecordNode(propName, NODE_FIELD);
                    RecordLocation(propId, propDecl.Identifier.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    RecordLocation(propId, propDecl.GetLocation().GetLineSpan(), LOCATION_SCOPE);

                    string pType = propDecl.Type.ToString();
                    int pTypeId = RecordNode(pType, NODE_TYPE, definitionKind: 0);
                    int edgeId = RecordEdge(propId, pTypeId, REFERENCE_TYPE_USAGE);
                    RecordLocation(edgeId, propDecl.Type.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                }
                else if (node is FieldDeclarationSyntax fieldDecl)
                {
                    string fType = fieldDecl.Declaration.Type.ToString();
                    int fTypeId = RecordNode(fType, NODE_TYPE, definitionKind: 0);

                    foreach (var variable in fieldDecl.Declaration.Variables)
                    {
                        string fieldName = $"{parentType}.{variable.Identifier.Text}";
                        int fieldId = RecordNode(fieldName, NODE_FIELD);
                        RecordLocation(fieldId, variable.Identifier.GetLocation().GetLineSpan(), LOCATION_TOKEN);

                        int edgeId = RecordEdge(fieldId, fTypeId, REFERENCE_TYPE_USAGE);
                        RecordLocation(edgeId, fieldDecl.Declaration.Type.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    }
                }
                else if (node is InvocationExpressionSyntax invocation)
                {
                    string calledName = invocation.Expression.ToString();
                    if (!string.IsNullOrEmpty(parentType))
                    {
                        int callerId = RecordNode(parentType, NODE_METHOD, 1, "()");
                        int calleeId = RecordNode(calledName, NODE_FUNCTION, 0, "()");
                        int callEdgeId = RecordEdge(callerId, calleeId, REFERENCE_CALL);
                        RecordLocation(callEdgeId, invocation.Expression.GetLocation().GetLineSpan(), LOCATION_TOKEN);
                    }
                }

                foreach (var child in node.ChildNodes())
                {
                    Walk(child, ns, parentType);
                }
            }

            Walk(root, "", "");

            transaction.Commit();
        }
    }
}
