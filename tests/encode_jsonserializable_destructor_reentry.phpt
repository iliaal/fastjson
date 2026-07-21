--TEST--
fastjson_encode: JsonSerializable result destructor may re-enter the outer object
--EXTENSIONS--
fastjson
--FILE--
<?php

class ReentryOuter implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return new ReentryResult($this);
    }
}

class ReentryResult {
    private static bool $entered = false;
    public function __construct(private ReentryOuter $outer) {}
    public function __destruct() {
        if (!self::$entered) {
            self::$entered = true;
            var_dump(fastjson_encode($this->outer));
            var_dump(fastjson_last_error() === JSON_ERROR_NONE);
        }
    }
}

var_dump(fastjson_encode(new ReentryOuter()));
var_dump(fastjson_last_error() === JSON_ERROR_NONE);
?>
--EXPECT--
string(2) "{}"
bool(true)
string(2) "{}"
bool(true)
