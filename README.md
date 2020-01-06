# bootymapper

Solution used to grab banners from TCP services and search their contents for a specific string.

`Usage: ./bootymapper [-c max_concurrent_sockets] [-t connection_timeout] [-r read_timeout] [-v verbosity=0-5] [-d send_data] [-s "search_string"] [-f ip_only] -m [max_read_size] -p port`

`-c` sets the maximum number of concurrent sockets that can be active at any one time. You should use `ulimit -n 1000000` to raise the kernel limit to one million file descriptors. The default is one million.

`-t` sets the timeout for a connection that has not completed a TCP handshake. The default is 5 seconds.

`-r` sets the timeout for a connection that has completed a TCP handshake but has not sent any data. The default is 5 seconds.

`-v` increases the verbosity level of bootymapper. Any informative messages are not printed to stdout.

`-d` sets the data file that bootymapper will send. A basic HTTP request is included in `request`. This is required.

`-s` sets the string bootymapper will search for. The default is to return any response.

`-f` allows you to set if the output is only the resulting IP or the IP and the contents of the response. The default is to print IP and contents.

`-m` sets the maximum read size of the response in megabytes. The default is 16 megabytes.

To install dependencies with apt use `apt-get install libevent-dev` and to compile bootymapper use `cd bootymapper && make`. You may need to update libevent from source to the latest version. You can find the latest libevent on https://github.com/libevent/libevent

IPv4 addresses can be generated with zmap or your preferred network mapper and piped directly into `bootymapper`.

Run the everything together as `zmap -p 80 | ./bootymapper -d ./request -p 80 -s "<title>test string</title>" -f ip_only -m 16 > test.txt` to scan port 80 with zmap and simultaneously grab all HTTP banners on port 80 and search for the specified string which is a title, printing only the found IPs, for example.

`request` contains the raw request which will be sent by bootymapper. For HTTP, this can be generated with echo: `echo -e "GET / HTTP/1.1\r\nHost: %s\r\n\r\n" > request`. bootymapper supports requests of all shapes and sizes.

`%s` in the file `request` is replaced automatically by `bootymapper` with the IP being scanned. For HTTP requests it can be changed to any specific hostname to return the contents of the endpoint on shared hosting solutions.
