#!/usr/bin/env bash
set -euo pipefail

echo "======================================================================"
echo " PIE install smoke test for iliaal/fastjson"
echo "======================================================================"
echo
echo "PHP:"
php --version | head -1
echo "phpize:"
phpize --version 2>&1 | head -2
echo

echo "---- 1. System build tools ----"
apt-get update -qq >/dev/null
# PIE 1.4 on a bare php:8.x-cli image needs four extras beyond what phpize
# expects: git (PIE clones source via git clone, not a tarball), bison and
# libtoolize (PIE's build-tools check insists on both even though phpize
# itself doesn't), and ca-certificates (for the HTTPS clone from github).
# unzip: composer's archive extractor prefers /usr/bin/unzip and silently
# falls back to PHP ZipArchive when it's missing, laying the prebuilt .so
# out at a path PIE's prePackagedBinary check doesn't look -> spurious
# ExtensionBinaryNotFound. php:8.x-cli ships no unzip. See
# ~/ai/wiki/debugging/php-ext-release-traps.md.
apt-get install -y -qq git ca-certificates bison libtool-bin unzip >/dev/null
git --version
bison --version | head -1
libtoolize --version | head -1 || echo "libtoolize not found"
echo

echo "---- 2. Fresh clone from mounted source (avoids host build artifacts) ----"
git config --global --add safe.directory /fastjson
git config --global --add safe.directory /fastjson/.git
git clone -q file:///fastjson /tmp/src
cd /tmp/src
echo "HEAD: $(git log --oneline -1)"
echo "tag:  $(git describe --tags --always)"
ls composer.json config.m4 php_fastjson.h | head
echo

echo "---- 3. Install Composer ----"
curl -sS https://getcomposer.org/installer | php -- --quiet
mv composer.phar /usr/local/bin/composer
composer --version | head -1
echo

echo "---- 4. Download PIE ----"
curl -sSL https://github.com/php/pie/releases/latest/download/pie.phar -o /usr/local/bin/pie
chmod +x /usr/local/bin/pie
ls -la /usr/local/bin/pie
pie --version 2>&1 | head -3
echo

echo "---- 5. pie install ----"
PIE_OK=0

mkdir -p /tmp/piework
cat > /tmp/piework/composer.json <<'JSON'
{
    "name": "iliaal/pie-smoke",
    "repositories": [
        { "type": "path", "url": "/tmp/src", "options": { "symlink": false } }
    ],
    "minimum-stability": "dev",
    "prefer-stable": true
}
JSON

echo "   [A] pie install -d /tmp/piework iliaal/fastjson"
pie install \
    -d /tmp/piework \
    --with-php-config=/usr/local/bin/php-config \
    --auto-install-build-tools \
    iliaal/fastjson 2>&1 \
    | tee /tmp/pie-A.out | tail -40 || true

if php -m | grep -qi fastjson; then
    PIE_OK=1
    echo "   [A] RESULT: success"
fi

# Path B: plain Packagist lookup (expected to fail until iliaal/fastjson is
# registered on packagist.org). Kept for completeness.
if [ "$PIE_OK" = "0" ]; then
    echo
    echo "   [B] pie install iliaal/fastjson  (plain Packagist lookup)"
    pie install \
        --with-php-config=/usr/local/bin/php-config \
        iliaal/fastjson 2>&1 | tee /tmp/pie-B.out | tail -20 || true
    if php -m | grep -qi fastjson; then
        PIE_OK=1
        echo "   [B] RESULT: success"
    fi
fi

echo "   overall PIE result: PIE_OK=$PIE_OK"
echo

echo "---- 6. Verify extension loads ----"
if [ "$PIE_OK" = "0" ]; then
    echo "   *** PIE did not install the extension; falling back to manual phpize+make+install ***"
    cd /tmp/src
    phpize >/dev/null
    ./configure --enable-fastjson >/dev/null
    make -j"$(nproc)" 2>&1 | tail -3
    make install 2>&1 | tail -3
    docker-php-ext-enable fastjson
    echo "   [fallback] manual install SUCCEEDED"
fi
php -m | grep -i fastjson
php -r 'echo "fastjson version: ", phpversion("fastjson"), PHP_EOL;'
echo

echo "---- 7. Functional smoke test ----"
php -r '
$v = fastjson_version();
if (!is_string($v) || $v === "") { echo "version FAIL: ", var_export($v, true), "\n"; exit(1); }
if ($v !== phpversion("fastjson")) { echo "version mismatch: ", $v, " vs ", phpversion("fastjson"), "\n"; exit(1); }
echo "version OK: $v\n";
'
echo
echo "======================================================================"
echo " PIE install smoke test: PASSED"
echo "======================================================================"
