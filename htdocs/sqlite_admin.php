<?php
$selectedTable = $_GET['table'] ?? ($tables[0] ?? null);

// Colonnes et données
$columns = [];
$rows = [];
if ($selectedTable && in_array($selectedTable, $tables)) {
    $columns = getColumns($db, $selectedTable);
    $res = getTableData($db, $selectedTable);
    while ($row = $res->fetchArray(SQLITE3_ASSOC)) {
        $rows[] = $row;
    }
}

// Requête SQL manuelle
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

<section class="sql-box">
    <h2>🧪 Requête SQL manuelle</h2>
    <?php if ($errorMessage): ?>
        <p class="error"><?= htmlspecialchars($errorMessage) ?></p>
    <?php elseif (is_string($queryResult)): ?>
        <p class="success"><?= htmlspecialchars($queryResult) ?></p>
    <?php endif; ?>

    <form method="post">
        <textarea name="sql_query" placeholder="Ex: SELECT * FROM tasks;"></textarea><br>
        <button type="submit">Exécuter</button>
    </form>

    <?php if (is_array($queryResult)): ?>
        <h3>Résultats :</h3>
        <table>
            <tr>
                <?php foreach (array_keys($queryResult[0] ?? []) as $col): ?>
                    <th><?= htmlspecialchars($col) ?></th>
                <?php endforeach; ?>
            </tr>
            <?php foreach ($queryResult as $row): ?>
                <tr>
                    <?php foreach ($row as $val): ?>
                        <td><?= htmlspecialchars($val) ?></td>
                    <?php endforeach; ?>
                </tr>
            <?php endforeach; ?>
        </table>
    <?php endif; ?>
</section>

   