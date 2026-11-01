<?php
// Run only inside the temporary test site created by server.test.cjs.
$fixture = json_decode(file_get_contents(__DIR__ . '/fixture.json'), true);
if (!defined('MYSQLI_REPORT_ERROR')) define('MYSQLI_REPORT_ERROR', 1);
if (!defined('MYSQLI_REPORT_STRICT')) define('MYSQLI_REPORT_STRICT', 2);
if (!function_exists('mysqli_report')) {
    function mysqli_report($flags) {}
}
class TestDatabase {
    public $row;
    public $statements = [];
    function __construct() {
        $this->row = $GLOBALS['fixture']['row'] ?? ['ID' => 440, 'price' => '1.00', 'department' => 'A1', 'grocery_item' => 'Milk', 'pantryqty' => '2'];
    }
    function prepare($sql) {
        if (!in_array($sql, [
            'UPDATE items SET price = ?, department = ? WHERE ID = ?',
            'UPDATE items SET price = ? WHERE ID = ?',
            'SELECT ID FROM items WHERE ID = ?'
        ], true)) throw new Exception('Unexpected SQL');
        $this->statements[] = $sql;
        return new TestStatement($this, $sql);
    }
}
class TestStatement {
    public $db;
    public $sql;
    public $values;
    public $affected_rows = 0;
    public $num_rows = 0;
    function __construct($db, $sql) { $this->db = $db; $this->sql = $sql; }
    function bind_param($types, &...$values) {
        $expected = strpos($this->sql, 'SELECT') === 0 ? 'i' : (strpos($this->sql, 'department') !== false ? 'ssi' : 'si');
        if ($types !== $expected || count($values) !== strlen($expected)) throw new Exception('Bad bindings');
        $this->values = $values;
    }
    function execute() {
        if ($GLOBALS['fixture']['fail'] ?? false) throw new Exception('Simulated database failure');
        $id = end($this->values);
        if ($this->db->row['ID'] !== $id) return;
        if (strpos($this->sql, 'SELECT') === 0) { $this->num_rows = 1; return; }
        $before = $this->db->row;
        $this->db->row['price'] = $this->values[0];
        if (strpos($this->sql, 'department') !== false) $this->db->row['department'] = $this->values[1];
        $this->affected_rows = $before === $this->db->row ? 0 : 1;
    }
    function store_result() {}
    function close() {}
}
$_SERVER['REQUEST_METHOD'] = $fixture['method'] ?? 'POST';
$_POST = $fixture['post'];
ob_start();
register_shutdown_function(function () {
    $response = ob_get_clean();
    echo json_encode([
        'status' => http_response_code(),
        'response' => json_decode($response, true),
        'row' => isset($GLOBALS['conn']) ? $GLOBALS['conn']->row : null,
        'statements' => isset($GLOBALS['conn']) ? $GLOBALS['conn']->statements : []
    ]);
});
require __DIR__ . '/ajax/edit_price.php';
