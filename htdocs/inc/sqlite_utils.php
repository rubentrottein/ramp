<?php
function getAvailableDatabases() {
    return [
        'todolist' => 'databases/todolist.db',
        'other_db' => 'databases/other_db.db',
    ];
}

function connectToDatabase($path) {
    if (!file_exists($path)) {
        die("❌ Base introuvable : $path");
    }
    return new SQLite3($path);
}

function getTables(SQLite3 $db) {
    $tables = [];
    $res = $db->query("SELECT name FROM sqlite_master WHERE type='table'");
    while ($row = $res->fetchArray(SQLITE3_ASSOC)) {
        $tables[] = $row['name'];
    }
    return $tables;
}

function getColumns(SQLite3 $db, $table) {
    $columns = [];
    $res = $db->query("PRAGMA table_info($table)");
    while ($col = $res->fetchArray(SQLITE3_ASSOC)) {
        $columns[] = $col['name'];
    }
    return $columns;
}

function getTableData(SQLite3 $db, $table, $limit = 100) {
    return $db->query("SELECT * FROM $table ORDER BY ROWID DESC LIMIT $limit");
}
