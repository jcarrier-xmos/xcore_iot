# NOTE:
# openssl is part of Windows 11, on prior versions mingw64 offers openssl.
# alternatively New-SelfSignedCertificate

$DURATION=365
$OUTPUT="mqtt_broker_certs"
#$SERVERNAME=$(([System.Net.Dns]::GetHostByName(($env:computerName))).Hostname)
#$SERVERIP="10.129.28.11"
$SERVERNAME="192.168.5.12"

if (-not(Test-Path $OUTPUT)) {
    New-Item -ItemType Directory $OUTPUT
}

#$XCN_OID_ENHANCED_KEY_USAGE = "2.5.29.37"
#$XCN_OID_SUBJECT_ALT_NAME2 = "2.5.29.17"
#$KEY_USAGE_OID_SECURE_EMAIL = "1.3.6.1.5.5.7.3.4"
#
#$CertParams = {
#    FriendlyName = 'XMOS XCORE IoT Test Certificate'
#    Subject = "CN=$SERVERNAME,O=XMOS,C=US,ST=NH,L=Hampton,OU=Eng,E=null"
#    TextExtension = @(
#        "$XCN_OID_ENHANCED_KEY_USAGE={text}$KEY_USAGE_OID_SECURE_EMAIL",
#        "$XCN_OID_SUBJECT_ALT_NAME2={text}DNS=$SERVERNAME&IPAddress=$SERVERIP")
#    KeyAlgorithm = 'RSA'
#    KeyLength = 2048
#    SmimeCapabilities = $true
#    NotAfter = [DateTime]::Now + [TimeSpan]::FromDays($DURATION)
#}
#
#New-SelfSignedCertificate -Type SSLServerAuthentication @CertParams

# See: https://mosquitto.org/man/mosquitto-tls-7.html

$GENERATE_SERVER_FILES=0
$GENERATE_CLIENT_FILES=0

if (-not(Test-Path("$OUTPUT/ca.key")) -or -not(Test-Path("$OUTPUT/ca.crt"))) {
    $GENERATE_SERVER_FILES=1
    $GENERATE_CLIENT_FILES=1

    # Generate a certificate authority certificate and key
    openssl genrsa -out $OUTPUT/ca.key 2048
    openssl req -new -x509 -days $DURATION -key $OUTPUT/ca.key -out $OUTPUT/ca.crt -subj "/C=US/ST=NH/L=Hampton/O=XMOS/OU=Eng/CN=ca/emailAddress=null"
}

if (-not(Test-Path("$OUTPUT/server.key")) -or -not(Test-Path("$OUTPUT/server.crt")) -or ($GENERATE_SERVER_FILES -eq 1)) {
    # Generate a server key without encryption
    openssl genrsa -out $OUTPUT/server.key 2048

    # Generate a certificate signing request to send to the CA
    openssl req -new -out $OUTPUT/server.csr -key $OUTPUT/server.key -subj "/C=US/ST=NH/L=Hampton/O=XMOS/OU=Eng/CN=$SERVERNAME/emailAddress=null"

    # Send the CSR to the CA, or sign it with your CA key
    openssl x509 -req -sha256 -in $OUTPUT/server.csr -CA $OUTPUT/ca.crt -CAkey $OUTPUT/ca.key -CAcreateserial -out $OUTPUT/server.crt -days $DURATION
}

if (-not(Test-Path("$OUTPUT/client.key")) -or -not(Test-Path("$OUTPUT/client.crt")) -or ($GENERATE_CLIENT_FILES -eq 1)) {
    # Generate a client key without encryption
    openssl genrsa -out $OUTPUT/client.key 2048

    # Generate a certificate signing request to send to the CA
    openssl req -new -out $OUTPUT/client.csr -key $OUTPUT/client.key -subj "/C=US/ST=NH/L=Hampton/O=XMOS/OU=Eng/CN=explorer/emailAddress=null"

    # Send the CSR to the CA, or sign it with your CA key
    openssl x509 -req -in  $OUTPUT/client.csr -CA  $OUTPUT/ca.crt -CAkey  $OUTPUT/ca.key -CAcreateserial -out  $OUTPUT/client.crt -days $DURATION
}

# xflash --quad-spi-clock 50MHz --factory build/example_freertos_iot.xe --boot-partition-size 0x100000 --data examples/freertos/iot/filesystem_support/example_freertos_iot_fat.fs