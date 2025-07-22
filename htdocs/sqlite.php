<?php
include('inc/header.php');
include('inc/sqlite_utils.php');
?>
<link rel="stylesheet" href="css/sqlite_styles.css">

<main>
    <h1>Interface d'administration SQLite</h1>

    <?php
    $databases = getAvailableDatabases();
    $currentDb = $_GET['db'] ?? $databases['todolist'];

    $db = connectToDatabase($currentDb);
    $tables = getTables($db);
    ?>

    <!-- Choix de la base -->
    <form method="get">
        <label for="databases">Choisir une base :</label>
        <select name="db" id="databases" onchange="this.form.submit()">
            <?php foreach ($databases as $name => $path): ?>
                <option value="<?= htmlspecialchars($path) ?>" <?= $path === $currentDb ? 'selected' : '' ?>>
                    <?= htmlspecialchars($name) ?>
                </option>
            <?php endforeach; ?>
        </select>
    </form>

    <!-- Liste des tables -->
    <section>
        <h3>Tables disponibles</h3>
        <article class="tables-list">
            <?php foreach ($tables as $t): ?>
                <a href="sqlite.php?db=<?= urlencode($currentDb) ?>&table=<?= urlencode($t) ?>">
                    <?= htmlspecialchars($t) ?>
                </a>
            <?php endforeach; ?>
        </article>
    </section>

    <!-- Affichage d'une table -->
    <?php if (!empty($_GET['table'])): ?>
        <?php
        $table = $_GET['table'];

        if (!in_array($table, $tables)) {
            echo "<p>❌ Table inconnue.</p>";
        } else {
            $columns = getColumns($db, $table);
            $rows = getTableData($db, $table);
        ?>
        <section>
            <h2>Table : <?= htmlspecialchars($table) ?></h2>
            <?php if (empty($columns)): ?>
                <p>⚠️ Aucune colonne trouvée.</p>
            <?php else: ?>
                <table border="1" cellpadding="8">
                    <tr>
                        <?php foreach ($columns as $col): ?>
                            <th><?= htmlspecialchars($col) ?></th>
                        <?php endforeach; ?>
                    </tr>
                    <?php while ($row = $rows->fetchArray(SQLITE3_ASSOC)): ?>
                        <tr>
                            <?php foreach ($columns as $col): ?>
                                <?php
                                    $value = $row[$col];
                                    if (in_array(strtolower($col), ['completed', 'done', 'active']) && is_numeric($value)) {
                                        $value = $value ? '✔️' : '❌';
                                    }
                                ?>
                                <td><?= htmlspecialchars($value) ?></td>
                            <?php endforeach; ?>
                        </tr>
                    <?php endwhile; ?>
                </table>
            <?php endif; ?>
        </section>
        <?php } ?>
    <?php endif; ?>

    <?php include('sqlite_admin.php'); ?>
    <?php $db->close(); ?>
</main>

<?php include('inc/footer.php'); ?>
