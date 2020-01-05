# bootymapper

`Usage: ./bootymapper [-c max_concurrent_sockets] [-t connection_timeout] [-r read_timeout] [-v verbosity=0-5] [-d send_data] [-s "search_string"] [-f ip_only] -p port`

Solution used to grab banners from TCP services and search their contents for a specific string.

To install dependencies with apt use `apt-get install libevent-dev libpcap-dev` and to compile type `cd bootymapper && make`.

IPv4 addresses can be generated with zmap or your preferred network mapper and piped directly into `bootymapper`.

Run the everything together as `zmap -p 80 | ./bootymapper -d ./request -p 80 -s "<title>test string</title>" -f ip_only > test.txt` to scan port 80 with zmap and simultaneously grab all HTTP banners on port 80 and search for the specified string which is a title, printing only the found IPs, for example.

`request` contains the raw request which can be generated with echo: `echo -e "GET / HTTP/1.1\r\nHost: %s\r\n\r\n" > request`. bootymapper supports requests of all shapes and sizes.

`%s` in the file `request` is replaced automatically by `bootymapper` with the IP being scanned. For HTTP requests it can be changed to any specific hostname to return the contents of the endpoint on shared hosting solutions.
