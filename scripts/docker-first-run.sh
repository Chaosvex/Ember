#!/bin/bash

cd "${0%/*}"
FILE=first-run.lock

if test -f "$FILE"; then
	./$1
	exit $?
fi

echo "Performing first run initialisation..."

DBCPAR=$(realpath ./tools/dbc-parser)
DBUTIL=$(realpath ./tools/dbutils)
SRPGEN=$(realpath ./tools/srpgen)

LOGIN_ROOT_PASS=$(cat $MYSQL_ROOT_PASSWORD_FILE)
EMBER_LOGIN_DB_PASS=$(cat $EMBER_LOGIN_PASSWORD_FILE)
WORLD_ROOT_PASS=$(cat $MYSQL_ROOT_PASSWORD_FILE)
WORLD_USER_DB_PASS=$(cat $EMBER_WORLD_PASSWORD_FILE)

echo "Performing initial database installation and updates..."
$DBUTIL --install login world --update login world --sql-dir sql/ \
        --login.root-user root --login.root-password $LOGIN_ROOT_PASS \
		--login.hostname db --login.set-user $EMBER_LOGIN_DB_USER --login.set-password $EMBER_LOGIN_DB_PASS \
		--login.db-name $EMBER_LOGIN_DB \
		--world.root-user root --world.root-password $WORLD_ROOT_PASS \
		--world.hostname db --world.set-user $EMBER_WORLD_DB_USER --world.set-password $WORLD_USER_DB_PASS \
		--world.db-name $EMBER_WORLD_DB \
		--shutup

if ! [ $? -eq 0 ]
then
	exit 1
fi

echo "Generating default DBCs..."
$DBCPAR -o dbcs/ -d dbcs/definitions/client/ dbcs/definitions/server/ --dbc-gen

echo "Adding a default user..."
OUT="$($SRPGEN -u $EMBER_DEFAULT_USER -p $EMBER_DEFAULT_PASS)"
let OUT1=0

# it's terrible but it's temporary
for i in $(echo $OUT); do
	if [ $OUT1 -eq 3 ]
	then
		VERIFIER=$i
	elif [ $OUT1 -eq 5 ]
	then
		SALT=$i
	fi
	let OUT1=$OUT1+1
done

# write the password to a file rather than put it on the command line
CNFFILE=".my.cnf"
touch $CNFFILE
trap 'rm -f "$CNFFILE"' EXIT
chmod 0600 "$CNFFILE"
cat >"$CNFFILE" <<EOF
[client]
user=${EMBER_LOGIN_DB_USER}
password=${EMBER_LOGIN_DB_PASS}
EOF
QUERY="INSERT INTO users (username, s, v, creation_date, subscriber, survey_request,  \
     pin_method, pin, totp_key) VALUES ('$EMBER_DEFAULT_USER', X'$SALT', '$VERIFIER', UTC_TIMESTAMP, \
	 b'1', b'0', b'0', b'0', b'0');"
mysql --defaults-extra-file=$CNFFILE -h db -D $EMBER_LOGIN_DB -e "$QUERY"

echo "Rewriting default MySQL configuration..."
sed -i "s/default_user/$EMBER_LOGIN_DB_USER/g" mysql_config.conf.dist
sed -i "s/default_password/$EMBER_LOGIN_DB_PASS/g" mysql_config.conf.dist
sed -i "s/default_database/$EMBER_LOGIN_DB/g" mysql_config.conf.dist
sed -i "s/127.0.0.1/db/g" mysql_config.conf.dist

echo "Installing default configs..."
for f in *.conf.dist; do
    mv -- "$f" "${f%.conf.dist}.conf"
done

# Launch!
touch first-run.lock
echo "Initialisation complete, starting server..."
./$1
exit $?