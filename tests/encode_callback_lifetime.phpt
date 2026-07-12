--TEST--
fastjson_encode destroys JsonSerializable temporaries after their value is encoded
--EXTENSIONS--
fastjson
--FILE--
<?php

final class LifetimeState {
    public int $destroyed = 0;
}

final class LifetimeTemporary {
    public function __construct(
        public int $value,
        private LifetimeState $state,
    ) {}

    public function __destruct() {
        $this->state->destroyed++;
    }
}

final class LifetimeSerializable implements JsonSerializable {
    public function __construct(private LifetimeState $state) {}

    public function jsonSerialize(): mixed {
        return new LifetimeTemporary($this->state->destroyed, $this->state);
    }
}

$nativeState = new LifetimeState();
$fastState = new LifetimeState();

$native = json_encode([
    new LifetimeSerializable($nativeState),
    new LifetimeSerializable($nativeState),
]);
$fast = fastjson_encode([
    new LifetimeSerializable($fastState),
    new LifetimeSerializable($fastState),
]);

var_dump($native);
var_dump($fast);
var_dump($fast === $native);
?>
--EXPECT--
string(25) "[{"value":0},{"value":1}]"
string(25) "[{"value":0},{"value":1}]"
bool(true)
