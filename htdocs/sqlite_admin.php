<?php
$dbPath = __DIR__ . '/databases/todolist.db';

if (!file_exists($dbPath)) {
    die("❌ Base de données introuvable à : $dbPath");
}

$db = new SQLite3($dbPath);

// Liste des tables
$tables = [];
$res = $db->query("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name ASC");
while ($row = $res->fetchArray(SQLITE3_ASSOC)) {
    $tables[] = $row['name'];
}

// Table sélectionnée
$selectedTable = $_GET['table'] ?? ($tables[0] ?? null);
$columns = [];
$rows = [];

if ($selectedTable) {
    $colRes = $db->query("PRAGMA table_info($selectedTable)");
    while ($col = $colRes->fetchArray(SQLITE3_ASSOC)) {
        $columns[] = $col['name'];
    }

    $dataRes = $db->query("SELECT * FROM $selectedTable LIMIT 100");
    while ($row = $dataRes->fetchArray(SQLITE3_ASSOC)) {
        $rows[] = $row;
    }
}

// Traitement de requête SQL manuelle
$queryResult = null;
$errorMessage = '';
if (!empty($_POST['sql_query'])) {
    $sql = trim($_POST['sql_query']);
    try {
        $res = $db->query($sql);
        if (stripos($sql, 'SELECT') === 0) {
            $queryResult = [];
            while ($row = $res->fetchArray(SQLITE3_ASSOC)) {
                $queryResult[] = $row;
            }
        } else {
            $db->exec($sql);
            $queryResult = "✅ Requête exécutée avec succès.";
        }
    } catch (Exception $e) {
        $errorMessage = "❌ Erreur SQL : " . $e->getMessage();
    }
}

$db->close();
?>

<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <title>Admin SQLite - Visualisateur</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background: #f5f6fa;
            padding: 20px;
        }
        h1 {
            text-align: center;
        }
        .tables-list {
            margin-bottom: 20px;
        }
        .tables-list a {
            margin-right: 10px;
            text-decoration: none;
            color: #007bff;
        }
        table {
            border-collapse: collapse;
            width: 100%;
            margin-bottom: 40px;
        }
        th, td {
            border: 1px solid #ccc;
            padding: 6px 10px;
        }
        th {
            background: #e9ecef;
        }
        .sql-box {
            margin-bottom: 30px;
            padding: 10px;
            background: #fff;
            border: 1px solid #ccc;
        }
        textarea {
            width: 100%;
            height: 80px;
            font-family: monospace;
            margin-bottom: 10px;
        }
        .error { color: red; }
        .success { color: green; }
    </style>
</head>
<body>
    <h1>🛠️ Admin SQLite</h1>

    <div class="tables-list">
        <strong>Tables disponibles :</strong><br>
        <?php foreach ($tables as $table): ?>
            <a href="?table=<?= urlencode($table) ?>"><?= htmlspecialchars($table) ?></a>
        <?php endforeach; ?>
    </div>

    <?php if ($selectedTable): ?>
        <h2>📋 Table : <?= htmlspecialchars($selectedTable) ?></h2>
        <table>
            <tr>
                <?php foreach ($columns as $col): ?>
                    <th><?= htmlspecialchars($col) ?></th>
                <?php endforeach; ?>
            </tr>
            <?php foreach ($rows as $row): ?>
                <tr>
                    <?php foreach ($columns as $col): ?>
                        <td><?= htmlspecialchars($row[$col] ?? '') ?></td>
                    <?php endforeach; ?>
                </tr>
            <?php endforeach; ?>
        </table>
    <?php endif; ?>

    <div class="sql-box">
        <h2>🧪 Exécuter une requête SQL</h2>
        <?php if ($errorMessage): ?>
            <p class="error"><?= htmlspecialchars($errorMessage) ?></p>
        <?php endif; ?>
        <?php if (is_string($queryResult)): ?>
            <p class="success"><?= htmlspecialchars($queryResult) ?></p>
        <?php endif; ?>
        <form method="POST">
            <textarea name="sql_query" placeholder="Ex: SELECT * FROM tasks;"></textarea>
            <button type="submit">Exécuter</button>
        </form>

        <?php if (is_array($queryResult)): ?>
            <h3>Résultat :</h3>
            <table>
                <tr>
                    <?php foreach (array_keys($queryResult[0] ?? []) as $col): ?>
                        <th><?= htmlspecialchars($col) ?></th>
                    <?php endforeach; ?>
                </tr>
                <?php foreach ($queryResult as $row): ?>
                    <tr>
                        <?php foreach ($row as $cell): ?>
                            <td><?= htmlspecialchars($cell) ?></td>
                        <?php endforeach; ?>
                    </tr>
                <?php endforeach; ?>
            </table>
        <?php endif; ?>
    </div>
</body>
</html>
