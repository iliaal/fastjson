--TEST--
fastjson_pointer_set: array index validation matches RFC 6901 / pointer_get (no leading zeros, no overflow)
--EXTENSIONS--
fastjson
--FILE--
<?php
/* The splice writer's index parser must agree with yyjson's ptr parser
 * used by pointer_get/_exists: a lone "0" is valid, but leading zeros
 * ("01"), overflowing digit runs, and the "-" append token do not
 * resolve to a settable array location. Otherwise exists() and set()
 * would disagree on the same pointer. */
$doc = '{"arr":[10,20,30]}';

foreach (['/arr/0', '/arr/1', '/arr/2', '/arr/01', '/arr/00', '/arr/-',
          '/arr/99999999999999999999999999', '/arr/3'] as $ptr) {
    $get = fastjson_pointer_get($doc, $ptr);
    $set = fastjson_pointer_set($doc, $ptr, 99);
    printf("%-32s get=%-5s set=%s\n", $ptr,
        $get === null ? "null" : json_encode($get),
        $set === false ? "false" : $set);
}
?>
--EXPECT--
/arr/0                           get=10    set={"arr":[99,20,30]}
/arr/1                           get=20    set={"arr":[10,99,30]}
/arr/2                           get=30    set={"arr":[10,20,99]}
/arr/01                          get=null  set=false
/arr/00                          get=null  set=false
/arr/-                           get=null  set=false
/arr/99999999999999999999999999  get=null  set=false
/arr/3                           get=null  set=false
