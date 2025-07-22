<?php
/**
 * Fichier de test complet pour Apache + PHP 8.3 + SQLite3
 * À placer dans C:\Apache24\htdocs\test.php
 */

// Configuration d'affichage des erreurs pour le test
error_reporting(E_ALL);
ini_set('display_errors', 1);

// Fonction pour afficher les résultats des tests
function displayTest($testName, $result, $details = '') {
    echo "<tr>";
    echo "<td><strong>$testName</strong></td>";
    if ($result) {
        echo "<td style='color: green;'>✓ OK</td>";
        echo "<td>$details</td>";
    } else {
        echo "<td style='color: red;'>✗ ERREUR</td>";
        echo "<td style='color: red;'>$details</td>";
    }
    echo "</tr>";
}

?>
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Test Serveur Apache + PHP 8.3 + SQLite3</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
            background: #f5f5f5;
        }
        .container {
            background: white;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            text-align: center;
            border-bottom: 3px solid #4CAF50;
            padding-bottom: 10px;
        }
        h2 {
            color: #2196F3;
            border-left: 4px solid #2196F3;
            padding-left: 15px;
            margin-top: 30px;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            margin: 15px 0;
        }
        th, td {
            padding: 10px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }
        th {
            background-color: #f2f2f2;
            font-weight: bold;
        }
        .info-box {
            background: #e3f2fd;
            border-left: 4px solid #2196F3;
            padding: 15px;
            margin: 15px 0;
        }
        .success-box {
            background: #e8f5e8;
            border-left: 4px solid #4CAF50;
            padding: 15px;
            margin: 15px 0;
        }
        .error-box {
            background: #ffebee;
            border-left: 4px solid #f44336;
            padding: 15px;
            margin: 15px 0;
        }
        pre {
            background: #f5f5f5;
            padding: 15px;
            border-radius: 5px;
            overflow-x: auto;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 Test du Serveur Apache + PHP 8.3 + SQLite3</h1>
        
        <div class="success-box">
            <strong>✓ Félicitations !</strong> Si vous voyez cette page, Apache et PHP fonctionnent correctement !
        </div>

        <h2>📋 Informations du serveur</h2>
        <table>
            <tr><th>Paramètre</th><th>Valeur</th></tr>
            <tr><td>Date/Heure</td><td><?= date('Y-m-d H:i:s') ?></td></tr>
            <tr><td>Serveur Web</td><td><?= $_SERVER['SERVER_SOFTWARE'] ?? 'Non défini' ?></td></tr>
            <tr><td>Version PHP</td><td><?= phpversion() ?></td></tr>
            <tr><td>Système d'exploitation</td><td><?= php_uname() ?></td></tr>
            <tr><td>Document Root</td><td><?= $_SERVER['DOCUMENT_ROOT'] ?? 'Non défini' ?></td></tr>
        </table>

        <h2>🧪 Tests des composants</h2>
        <table>
            <tr><th>Test</th><th>Statut</th><th>Détails</th></tr>
            <?php
            // Test de la version PHP
            $phpVersion = phpversion();
            $phpOk = version_compare($phpVersion, '8.0.0', '>=');
            displayTest('Version PHP', $phpOk, "Version: $phpVersion");

            // Test de SQLite3 (classe)
            $sqlite3Ok = class_exists('SQLite3');
            displayTest('SQLite3 (Classe)', $sqlite3Ok, $sqlite3Ok ? 'Extension SQLite3 chargée' : 'Extension SQLite3 non disponible');

            // Test de PDO SQLite
            $pdoSqliteOk = in_array('sqlite', PDO::getAvailableDrivers());
            displayTest('PDO SQLite', $pdoSqliteOk, $pdoSqliteOk ? 'Driver PDO SQLite disponible' : 'Driver PDO SQLite non disponible');

            // Test des extensions importantes
            $extensions = ['mbstring', 'json', 'session', 'curl', 'openssl'];
            foreach ($extensions as $ext) {
                $loaded = extension_loaded($ext);
                displayTest("Extension $ext", $loaded, $loaded ? 'Chargée' : 'Non disponible');
            }

            // Test d'écriture dans le répertoire temporaire
            $tmpDir = sys_get_temp_dir();
            $testFile = $tmpDir . '/test_write.txt';
            $writeOk = @file_put_contents($testFile, 'test') !== false;
            if ($writeOk) @unlink($testFile);
            displayTest('Écriture temporaire', $writeOk, $writeOk ? "Répertoire: $tmpDir" : 'Impossible d\'écrire dans le répertoire temporaire');
            ?>
        </table>

        <h2>💾 Test de SQLite3</h2>
        <?php
        try {
            // Création d'une base de données SQLite en mémoire pour le test
            $db = new SQLite3(':memory:');
            
            // Création d'une table de test
            $db->exec('CREATE TABLE test (id INTEGER PRIMARY KEY, nom TEXT, date_creation DATETIME DEFAULT CURRENT_TIMESTAMP)');
            
            // Insertion de données de test
            $stmt = $db->prepare('INSERT INTO test (nom) VALUES (?)');
            $stmt->bindValue(1, 'Test SQLite3');
            $result = $stmt->execute();
            
            // Lecture des données
            $query = $db->query('SELECT * FROM test');
            $data = $query->fetchArray(SQLITE3_ASSOC);
            
            echo '<div class="success-box">';
            echo '<strong>✓ SQLite3 fonctionne parfaitement !</strong><br>';
            echo 'Version SQLite: ' . SQLite3::version()['versionString'] . '<br>';
            echo 'Données insérées et lues avec succès:<br>';
            echo '<pre>' . print_r($data, true) . '</pre>';
            echo '</div>';
            
            $db->close();
            
        } catch (Exception $e) {
            echo '<div class="error-box">';
            echo '<strong>✗ Erreur SQLite3:</strong> ' . $e->getMessage();
            echo '</div>';
        }
        ?>

        <h2>🔧 Test de PDO SQLite</h2>
        <?php
        try {
            // Test de PDO avec SQLite
            $pdo = new PDO('sqlite::memory:');
            $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
            
            // Création d'une table
            $pdo->exec('CREATE TABLE users (id INTEGER PRIMARY KEY, username TEXT, email TEXT)');
            
            // Insertion avec prepared statement
            $stmt = $pdo->prepare('INSERT INTO users (username, email) VALUES (?, ?)');
            $stmt->execute(['admin', 'admin@localhost']);
            
            // Lecture des données
            $stmt = $pdo->query('SELECT * FROM users');
            $users = $stmt->fetchAll(PDO::FETCH_ASSOC);
            
            echo '<div class="success-box">';
            echo '<strong>✓ PDO SQLite fonctionne parfaitement !</strong><br>';
            echo 'Utilisateur créé et lu avec succès:<br>';
            echo '<pre>' . print_r($users, true) . '</pre>';
            echo '</div>';
            
        } catch (PDOException $e) {
            echo '<div class="error-box">';
            echo '<strong>✗ Erreur PDO SQLite:</strong> ' . $e->getMessage();
            echo '</div>';
        }
        ?>

        <h2>📁 Structure des fichiers recommandée</h2>
        <div class="info-box">
            <strong>Répertoires importants :</strong><br>
            • <code>C:\Apache24\htdocs\</code> - Vos fichiers web<br>
            • <code>C:\Apache24\databases\</code> - Vos bases de données SQLite<br>
            • <code>C:\Apache24\logs\</code> - Logs d'Apache et PHP<br>
            • <code>C:\Apache24\tmp\</code> - Fichiers temporaires<br><br>
            
            <strong>Fichiers de configuration :</strong><br>
            • <code>C:\Apache24\conf\httpd.conf</code> - Configuration Apache<br>
            • <code>C:\Apache24\php\php.ini</code> - Configuration PHP
        </div>

        <h2>🔗 Liens utiles</h2>
        <ul>
            <li><a href="phpinfo.php" target="_blank">Voir les informations PHP complètes (phpinfo)</a></li>
            <li><a href="app.php" target="_blank">Tester l'application exemple avec SQLite</a></li>
            <li><a href="/" target="_blank">Retour à l'accueil</a></li>
        </ul>

        <h2>📚 Prochaines étapes</h2>
        <div class="info-box">
            1. Créez vos applications dans <code>C:\Apache24\htdocs\</code><br>
            2. Utilisez SQLite3 ou PDO pour vos bases de données<br>
            3. Consultez les logs en cas de problème dans <code>C:\Apache24\logs\</code><br>
            4. Modifiez <code>php.ini</code> selon vos besoins spécifiques
        </div>
    </div>
</body>
</html>