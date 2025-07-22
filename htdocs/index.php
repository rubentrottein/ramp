<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Test Serveur</title>
    <style>
        * { box-sizing: border-box; }
        body { font-family: Arial, sans-serif; background: #f4f4f4; color: #333; margin: 0; padding: 20px; }
        .success { color: green; }
        .error { color: red; }
        code { background: #f4f4f4; padding: 10px; display: block; margin: 10px 0; }
        .buttons{ display:flex; flex-wrap: wrap; gap: 10px; }
        .buttons a{
            padding: 10px 20px;
            background-color: steelblue;
            color: white;
            border: none;
            text-decoration: none;
            border-radius: 5px;
            cursor: pointer;
        }
        header{
            text-align: center;
            margin-bottom: 20px;
        }
        main{
            background: #f9f9f9;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
            width: 80%;
            margin: auto;
        }
        footer{
            margin-top: 20px;
            padding: 10px;
            text-align: center;
            background-color: #f1f1f1;
            border-top: 1px solid #ccc;
        }
    </style>
</head>
<body>
    <header>
        <h1>Test Serveur Apache + PHP 8.3 + SQLite3</h1>
        <p>Ce script teste la configuration du serveur Apache, de PHP et de SQLite3.</p>
        <p>Current date and time: <?php echo date('Y-m-d H:i:s'); ?></p>
    </header>
    <main>
        
        <h2>Informations Système</h2>
        <p>Server name: <?php echo $_SERVER['SERVER_NAME']; ?></p>
        <p>PHP version: <?php echo phpversion(); ?></p>
        <p>Server software: <?php echo $_SERVER['SERVER_SOFTWARE']; ?></p>
        <p>Document root: <?php echo $_SERVER['DOCUMENT_ROOT']; ?></p>

        <h3>Boutons de test</h3>
        <article class="buttons">
            <a href="index.html">Index Général ❤</a>
            <a href="info.php">Info 🚧</a>
            <a href="test.php">PHP Test 🧪</a>
            <a href="sqlite.php">SQLite3 Test 🧪</a>
            <a href="sqlite_admin.php">SQLite3 Admin ⚠</a>
            <a href="todo.php">Application To-do List php/Sqlite3 💱</a>
        </article>

    <h1>Test du Serveur</h1>


        <h2>1. Test Apache</h2>
        <?php 
        echo "PHP Version: " . phpversion() . "<br>";
        
        if (function_exists('apache_get_modules')) {
            echo "Apache Version: " . $_SERVER['SERVER_SOFTWARE'];
            echo "<p><b>Apache fonctionnel et démarré correctement.✅</b><p>";
        } else {
            echo "Apache Version: " . (isset($_SERVER['SERVER_SOFTWARE']) ? $_SERVER['SERVER_SOFTWARE'] : 'Non détectée');
        }
        ?>
        <h2>2. Test PHP</h2>
        <?php
        echo "PHP Version: " . PHP_VERSION . "<br>";
        echo "Loaded extensions: " . implode(', ', get_loaded_extensions()) . "<br>";
        echo "SQLite3 loaded: " . (extension_loaded('sqlite3') ? 'Yes' : 'No') . "<br>";
        echo "PHP ini loaded file: " . php_ini_loaded_file() . "<br>";
        echo "PHP ini scan dir: " . php_ini_scanned_files() . "<br>";
        echo "<b>PHP fonctionnel et démarré correctement ✅<br></b>";
        ?>
        <h2>3. Test SQLite</h2>
        <p><i>Ici apparaîtront les tables SQL si SQLite fonctionne bien</i></p>
        <?php
        $dbPath = __DIR__ . '/databases/todolist.db';
        if (!file_exists($dbPath)) {
            die("Database not found: $dbPath");
        }
        $db = new SQLite3($dbPath);
        $result = $db->query('SELECT * FROM tasks ORDER BY created_at DESC');
        echo "<h1>Task List (SQLite)</h1>";
        echo "<table border='1' cellpadding='8'>";
        echo "<tr><th>ID</th><th>Title</th><th>Description</th>Completed</th><th>Created At</th></tr>";
        while ($row = $result->fetchArray(SQLITE3_ASSOC)) {
            echo "<tr>";
            echo "<td>{$row['id']}</td>";
            echo "<td>{$row['title']}</td>";
            echo "<td>{$row['description']}</td>";
            echo "<td>" . ($row['completed'] ? '✔️' : '❌') . "</td>";
            echo "<td>{$row['created_at']}</td>";
            echo "</tr>";
        }
        echo "</table>";
        $db->close();
        echo "<p><b>SQLite fonctionne correctement ✅</b></p>";
        ?>
    </main>
    <footer>
        <p>&copy; 2023 Test Serveur. All rights reserved.</p>
    </footer>
</body>
</html>