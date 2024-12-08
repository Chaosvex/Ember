# 🔥 **SRP6 Credentials Generator**
---

This is a simple tool for generating SRP6 credentials for storing in the database. Simply provide a username and password on the command line and enter the provided details into the accounts table.

For example:

`srpgen -u MyUser -p MyPass -s`

The `-s` flag will place the salt in a binary file in addition to printing out the hexadecimal form.

The account service is intended to make this tool obsolete in the future.