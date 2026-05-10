--TEST--
Whitespace part of bug #64874 ("fastjson_decode handles whitespace and case-sensitivity incorrectly")
--EXTENSIONS--
fastjson
--FILE--
<?php
function decode($json) {
    var_dump(fastjson_decode($json));
    var_dump(fastjson_last_error() !== 0);
    echo "\n";
}

// Leading whitespace should be ignored
decode(" true");
decode("\ttrue");
decode("\ntrue");
decode("\rtrue");

// So should trailing whitespace
decode("true ");
decode("true\t");
decode("true\n");
decode("true\r");

// And so should the combination of both
decode(" true ");
decode(" true\t");
decode(" true\n");
decode(" true\r");
decode("\ttrue ");
decode("\ttrue\t");
decode("\ttrue\n");
decode("\ttrue\r");
decode("\ntrue ");
decode("\ntrue\t");
decode("\ntrue\n");
decode("\ntrue\r");
decode("\rtrue ");
decode("\rtrue\t");
decode("\rtrue\n");
decode("\rtrue\r");

echo "Done\n";
?>
--EXPECT--
bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

bool(true)
bool(false)

Done
