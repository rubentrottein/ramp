<?php include('header.php'); ?>
<link rel="stylesheet" href="css/sqlite_styles.css">

<main>
    <h2> Lister toutes les tables </h2>
    <?php
    // choisir la base de données 
    $databases = [
        'todolist' => 'databases/todolist.db',
        'other_db' => 'databases/other_db.db', // Add more databases as needed
    ];
    ?>
    <select name="databases" id="databases">
        <?php foreach ($databases as $name => $path): ?>
            <option value="<?php echo htmlspecialchars($path); ?>"><?php echo htmlspecialchars($name); ?></option>
        <?php endforeach; ?>
    </select>
    <button id="load-tables">Charger les tables</button>
    <script>
        document.getElementById('load-tables').addEventListener('click', function() {
            const dbPath = document.getElementById('databases').value;
            window.location.href = 'sqlite.php?db=' + encodeURIComponent(dbPath);
        });
    </script>


    <section>

        <h2>Visualisateur de table</h2>
        
        <?php

            $dbPath = isset($_GET['db']) ? $_GET['db'] : 'databases/todolist.db';
            if (!file_exists($dbPath)) {
                die("Database not found: $dbPath");
            }
            $db = new SQLite3($dbPath);
            if (!$db) {
                die("Could not connect to the database.");
            }

            echo "<h3>Tables dans la base de données :</h3>";

            $tables = $db->query("SELECT name FROM sqlite_master WHERE type='table'");
            echo "<article class='tables-list'>";
            while ($table = $tables->fetchArray(SQLITE3_ASSOC)) {
                echo "<a href='?table=" . htmlspecialchars($table['name']) . "'>" . htmlspecialchars($table['name']) . "</a>";
            }
            echo "</article>";
            //close the database connection
            function db_close($db) {
                if ($db) {
                    $db->close();
                }
            }
        ?>
        <?php
            $tables = [];
            $res = $db->query("SELECT name FROM sqlite_master WHERE type='table'");
            while ($row = $res->fetchArray(SQLITE3_ASSOC)) {
                $tables[] = $row['name'];
            }
        ?>
        <h3>AnyTable</h3>
        <article class="beans-list">
            <p>Choisissez une table pour afficher son contenu :</p>
            
            <?php
                $table = $_GET['table'] ?? 'tasks';
                $res = $db->query("PRAGMA table_info($table)");
                while ($col = $res->fetchArray(SQLITE3_ASSOC)) {
                    echo "<span class='column-name'>" . htmlspecialchars($col['name']) . "</span> ";
                }

                $res = $db->query("SELECT * FROM $table LIMIT 100");
                while ($row = $res->fetchArray(SQLITE3_ASSOC)) {
                    // Affiche les lignes
                }
                ?>
        </article>
    </section>
        
    <section>
        <h2>Table : <?= $table ?></h2>
        <?php
            $dbPath = __DIR__ . '/databases/todolist.db';

            if (!file_exists($dbPath)) {
                die("❌ Base de données introuvable : $dbPath");
            }

            $db = new SQLite3($dbPath);

            // Table cible
            $table = $_GET['table'] ?? 'tasks';

            // Récupère les colonnes dynamiquement
            $columns = [];
            $res = $db->query("PRAGMA table_info($table)");
            while ($col = $res->fetchArray(SQLITE3_ASSOC)) {
                $columns[] = $col['name'];
            }

            // Requête principale
            $result = $db->query("SELECT * FROM $table ORDER BY ROWID DESC");

            // Affichage du tableau
            echo "<table border='1' cellpadding='8'>";
            echo "<tr>";
            foreach ($columns as $col) {
                echo "<th>" . htmlspecialchars($col) . "</th>";
            }
            echo "</tr>";

            // Données
            while ($row = $result->fetchArray(SQLITE3_ASSOC)) {
                echo "<tr>";
                foreach ($columns as $col) {
                    $value = $row[$col];

                    // Formatage pour colonnes booléennes
                    if (in_array(strtolower($col), ['completed', 'done', 'active']) && is_numeric($value)) {
                        $value = $value ? '✔️' : '❌';
                    }

                    echo "<td>" . htmlspecialchars($value) . "</td>";
                }
                echo "</tr>";
            }
            echo "</table>";

            $db->close();
            ?>

    </section>
</main>

<?php include('footer.php'); ?>