<?php
/**
 * Exemple d'application web simple avec SQLite3
 * Gestionnaire de tâches (Todo List) complet
 * À placer dans C:\Apache24\htdocs\app.php
 */

// Configuration
$dbPath = __DIR__ . '/databases/todolist.db';
$dbDir = dirname($dbPath);

// Créer le dossier des bases de données s'il n'existe pas
if (!is_dir($dbDir)) {
    mkdir($dbDir, 0777, true);
}

// Fonction pour initialiser la base de données
function initDatabase($dbPath) {
    $db = new SQLite3($dbPath);
    
    // Créer la table si elle n'existe pas
    $db->exec('
        CREATE TABLE IF NOT EXISTS tasks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            description TEXT,
            completed INTEGER DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    ');
    
    // Ajouter quelques tâches d'exemple si la table est vide
    $count = $db->querySingle('SELECT COUNT(*) FROM tasks');
    if ($count == 0) {
        $stmt = $db->prepare('INSERT INTO tasks (title, description, completed) VALUES (?, ?, ?)');
        
        $examples = [
            ['Configurer Apache + PHP', 'Installation et configuration du serveur web local', 1],
            ['Tester SQLite3', 'Vérifier que la base de données fonctionne correctement', 1],
            ['Créer une application web', 'Développer une première application avec SQLite', 0],
            ['Apprendre PHP 8.3', 'Explorer les nouvelles fonctionnalités de PHP 8.3', 0]
        ];
        
        foreach ($examples as $example) {
            $stmt->bindValue(1, $example[0]);
            $stmt->bindValue(2, $example[1]);
            $stmt->bindValue(3, $example[2]);
            $stmt->execute();
        }
    }
    
    return $db;
}

// Traitement des actions POST
$message = '';
$messageType = '';

if ($_POST) {
    $db = initDatabase($dbPath);
    
    try {
        if (isset($_POST['action'])) {
            switch ($_POST['action']) {
                case 'add':
                    $title = trim($_POST['title'] ?? '');
                    $description = trim($_POST['description'] ?? '');
                    
                    if (!empty($title)) {
                        $stmt = $db->prepare('INSERT INTO tasks (title, description) VALUES (?, ?)');
                        $stmt->bindValue(1, $title);
                        $stmt->bindValue(2, $description);
                        $stmt->execute();
                        $message = "Tâche '$title' ajoutée avec succès !";
                        $messageType = 'success';
                    } else {
                        $message = "Le titre de la tâche est obligatoire.";
                        $messageType = 'error';
                    }
                    break;
                    
                case 'toggle':
                    $id = intval($_POST['id'] ?? 0);
                    if ($id > 0) {
                        $stmt = $db->prepare('UPDATE tasks SET completed = NOT completed, updated_at = CURRENT_TIMESTAMP WHERE id = ?');
                        $stmt->bindValue(1, $id);
                        $stmt->execute();
                        $message = "Statut de la tâche mis à jour !";
                        $messageType = 'success';
                    }
                    break;
                    
                case 'delete':
                    $id = intval($_POST['id'] ?? 0);
                    if ($id > 0) {
                        $stmt = $db->prepare('DELETE FROM tasks WHERE id = ?');
                        $stmt->bindValue(1, $id);
                        $stmt->execute();
                        $message = "Tâche supprimée avec succès !";
                        $messageType = 'success';
                    }
                    break;
            }
        }
        $db->close();
    } catch (Exception $e) {
        $message = "Erreur : " . $e->getMessage();
        $messageType = 'error';
    }
    
    // Redirection pour éviter le resubmit
    header('Location: ' . $_SERVER['PHP_SELF'] . ($message ? '?msg=' . urlencode($message) . '&type=' . $messageType : ''));
    exit;
}

// Récupération des messages de redirection
if (isset($_GET['msg'])) {
    $message = $_GET['msg'];
    $messageType = $_GET['type'] ?? 'info';
}

// Récupération des tâches
$db = initDatabase($dbPath);
$tasks = [];
$stats = ['total' => 0, 'completed' => 0, 'pending' => 0];

try {
    // Statistiques
    $stats['total'] = $db->querySingle('SELECT COUNT(*) FROM tasks');
    $stats['completed'] = $db->querySingle('SELECT COUNT(*) FROM tasks WHERE completed = 1');
    $stats['pending'] = $stats['total'] - $stats['completed'];
    
    // Récupération des tâches triées par date de création
    $result = $db->query('SELECT * FROM tasks ORDER BY completed ASC, created_at DESC');
    while ($row = $result->fetchArray(SQLITE3_ASSOC)) {
        $tasks[] = $row;
    }
} catch (Exception $e) {
    $message = "Erreur lors de la récupération des tâches : " . $e->getMessage();
    $messageType = 'error';
}

$db->close();
?>

<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Todo List - Application SQLite3</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        
        .container {
            max-width: 800px;
            margin: 0 auto;
            background: white;
            border-radius: 15px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            overflow: hidden;
        }
        
        .header {
            background: linear-gradient(135deg, #2196F3, #21CBF3);
            color: white;
            padding: 30px;
            text-align: center;
        }
        
        .header h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
        }
        
        .header p {
            opacity: 0.9;
            font-size: 1.1em;
        }
        
        .stats {
            display: flex;
            justify-content: space-around;
            padding: 20px;
            background: #f8f9fa;
            border-bottom: 1px solid #e9ecef;
        }
        
        .stat-item {
            text-align: center;
        }
        
        .stat-number {
            font-size: 2em;
            font-weight: bold;
            color: #495057;
        }
        
        .stat-label {
            color: #6c757d;
            margin-top: 5px;
        }
        
        .content {
            padding: 30px;
        }
        
        .message {
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 20px;
            font-weight: 500;
        }
        
        .message.success {
            background: #d4edda;
            border: 1px solid #c3e6cb;
            color: #155724;
        }

        .message.error {
            background: #f8d7da;
            border: 1px solid #f5c6cb;
            color: #721c24;
        }

        form.add-task {
            display: flex;
            flex-direction: column;
            gap: 10px;
            margin-bottom: 30px;
        }

        form.add-task input,
        form.add-task textarea {
            padding: 10px;
            border: 1px solid #ccc;
            border-radius: 8px;
            font-size: 1em;
        }

        form.add-task button {
            padding: 10px;
            background: #28a745;
            color: white;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            font-weight: bold;
        }

        .task-list {
            list-style: none;
            display: flex;
            flex-direction: column;
            gap: 15px;
        }

        .task {
            padding: 15px;
            background: #f1f3f5;
            border-radius: 10px;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .task.completed {
            text-decoration: line-through;
            color: #6c757d;
            background: #d4edda;
        }

        .task .actions form {
            display: inline;
        }

        .task .actions button {
            background: none;
            border: none;
            color: #007bff;
            font-weight: bold;
            cursor: pointer;
            margin-left: 10px;
        }

        .task .actions button.delete {
            color: #dc3545;
        }
        nav {
            margin: 20px 0;
            text-align: center;
        }
        nav a {
            margin: 0 10px;
            color: #007bff;
            text-decoration: none;
            font-weight: bold;
        }
        nav a:hover {
            text-decoration: underline;
        }
        footer {
            text-align: center;
            padding: 20px;
            background: #f8f9fa;
            border-top: 1px solid #e9ecef;
            margin-top: 20px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Ma Todo List</h1>
            <p>Gérez facilement vos tâches avec PHP & SQLite3</p>
            <nav>
                <a href="index.php">Accueil</a>
                <a href="info.php">Info</a>
                <a href="test.php">Test PHP</a>
                <a href="sqlite.php">Test SQLite3</a>
                <a href="todo.php">Todo List</a>
            </nav>
        </div>

        <div class="stats">
            <div class="stat-item">
                <div class="stat-number"><?= $stats['total'] ?></div>
                <div class="stat-label">Total</div>
            </div>
            <div class="stat-item">
                <div class="stat-number"><?= $stats['completed'] ?></div>
                <div class="stat-label">Terminées</div>
            </div>
            <div class="stat-item">
                <div class="stat-number"><?= $stats['pending'] ?></div>
                <div class="stat-label">À faire</div>
            </div>
        </div>

        <div class="content">
            <?php if ($message): ?>
                <div class="message <?= htmlspecialchars($messageType) ?>">
                    <?= htmlspecialchars($message) ?>
                </div>
            <?php endif; ?>

            <form class="add-task" method="post">
                <input type="hidden" name="action" value="add">
                <input type="text" name="title" placeholder="Titre de la tâche" required>
                <textarea name="description" placeholder="Description (optionnelle)" rows="2"></textarea>
                <button type="submit">Ajouter la tâche</button>
            </form>

            <ul class="task-list">
                <?php foreach ($tasks as $task): ?>
                    <li class="task <?= $task['completed'] ? 'completed' : '' ?>">
                        <div>
                            <strong><?= htmlspecialchars($task['title']) ?></strong><br>
                            <small><?= htmlspecialchars($task['description']) ?></small>
                        </div>
                        <div class="actions">
                            <form method="post" style="display:inline;">
                                <input type="hidden" name="action" value="toggle">
                                <input type="hidden" name="id" value="<?= $task['id'] ?>">
                                <button type="submit"><?= $task['completed'] ? 'Marquer comme non terminée' : 'Terminer' ?></button>
                            </form>
                            <form method="post" style="display:inline;">
                                <input type="hidden" name="action" value="delete">
                                <input type="hidden" name="id" value="<?= $task['id'] ?>">
                                <button type="submit" class="delete">Supprimer</button>
                            </form>
                        </div>
                    </li>
                <?php endforeach; ?>
            </ul>
        </div>
    </div>
</body>
</html>
