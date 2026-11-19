<?php
/* The deferred memory-limit error (ZEND_HAS_OOM) shipped in PHP 8.2.35,
 * 8.3.36, 8.4.27, 8.5.12, and 8.6.0. Older engines bail out inside the
 * allocation, so these tests would exercise the engine, not fastjson. */
$deferred = PHP_VERSION_ID >= 80600
    || (PHP_VERSION_ID >= 80512 && PHP_VERSION_ID < 80600)
    || (PHP_VERSION_ID >= 80427 && PHP_VERSION_ID < 80500)
    || (PHP_VERSION_ID >= 80336 && PHP_VERSION_ID < 80400)
    || (PHP_VERSION_ID >= 80235 && PHP_VERSION_ID < 80300);
if (!$deferred) {
    print 'skip requires deferred memory-limit errors (PHP 8.2.35+, 8.3.36+, 8.4.27+, 8.5.12+, 8.6)';
} elseif (!is_readable('/proc/self/maps')) {
    print 'skip requires Linux procfs';
} elseif (!function_exists('proc_open')) {
    print 'skip proc_open unavailable';
}
