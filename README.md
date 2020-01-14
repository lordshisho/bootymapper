# bootymapper

Solution used to grab banners from TCP services and search their contents for a specific string.

Be your own Shodan. Be your own Censys. Map the Internet and find the booty.

### Usage and Arguments

`Usage: ./bootymapper [-c max_concurrent_sockets] [-t connection_timeout] [-r read_timeout] [-v verbosity=0-5] [-s "search_string"] [-f ip_only] -p port -d request -m max_read_size`

`-c` sets the maximum number of concurrent sockets that can be active at any one time. You should use `ulimit -n` to raise the default kernel limit of 1024. The default is 10000.

`-t` sets the timeout for a connection that has not completed a TCP handshake. The default is 5 seconds.

`-r` sets the timeout for a connection that has completed a TCP handshake but has not sent any data. This timeout is also used to timeout connections that have finished sending data. The default is 5 seconds.

`-v` increases the verbosity level of bootymapper. Any informative messages are not printed to stdout.

`-s` sets the string bootymapper will search for. The default is to not search for anything and return all responses.

`-f` allows you to set the output to the resulting IP or the IP and the contents of a response. The default is to print IP and contents.

`-p` sets the destination port where the request is sent. This is required.

`-d` sets the data file that bootymapper will send on a successful connection. A basic HTTP request is included in `request`. This is required.

`-m` sets the max read size of a single socket. The default is 1 megabyte or 1048576 bytes.

### Installation and Compilation

You may need to compile and install libevent from source to update it to the latest version. You can find the latest libevent on https://github.com/libevent/libevent

Use `cd bootymapper && make` to compile bootymapper.

### Examples

IPv4 addresses can be generated with zmap or your preferred network mapper and piped directly into bootymapper.

For example, use `zmap -p 80 | ./bootymapper -d ./request -p 80 -s "<title>Test Title, Please Ignore</title>" -m 100000 > test.txt` to scan port 80 with zmap and simultaneously grab all banners and search them for the specified string which includes title tags up to a max read size of 100000 bytes writing the resulting IP and contents to test.txt if the string is found.

`request` contains the raw request which will be sent by bootymapper. For HTTP, this can be generated with echo: `echo -e "GET / HTTP/1.1\r\nHost: %s\r\n\r\n" > request`. bootymapper supports requests of all shapes and sizes.

Including `%s` in the file `request` causes the `%s` to be replaced automatically by bootymapper with the IP being scanned. For HTTP requests it can be changed to any specific hostname to return the contents of the endpoint on shared hosting solutions.
