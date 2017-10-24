# Powerful chunked storage service with libevent and leveldb
Used to store any large file with minimum network failure. All file (chunk) are stored in leveldb with unique key.

example :
we have a file.pdf with 1 GB size, the chunk size is 1 MB. so we have 1000 chunks stored in leveldb + 1 metadata. The identifier key is generated at first chunk upload. the main identifier key for a file is time_millis.filename

So all the key stored in leveldb are :
```
1508834228933.file.pdf.meta
1508834228933.file.pdf.0
1508834228933.file.pdf.1
1508834228933.file.pdf.999
```

all file information such as filename, number of chunks, filesize, and mimetype are stored in meta value.

### External
external sources :

* ![Leveldb](https://github.com/google/leveldb)
```bash
sudo apt install libleveldb-dev
```
* ![Libevent](https://github.com/nmathewson/Libevent)

### Compile
```bash
make
```

### Run

#### 1. Run the service
```bash
./chunkedstorage [port]
./chunkedstorage 7000
```
#### 2 Open in browser

* Open in your browser client/index.html
* Select file

### nginx integration

If you are running you website on port ```80``` with domain ```storage.hello.com```. you can use nginx reverse proxy

```
server {
    listen 80;
    server_name storage.hello.com *.storage.hello.com;

	location / {
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header   Host      $http_host;
        proxy_pass         http://127.0.0.1:7000;
    }
}
```
Then you can change the server variable on client/js/script.js to storage.hello.com
Don't forget to block direct access to these opened port (7000) with iptables, see https://github.com/aripnk/iptables_configurator



# License

MIT License

Copyright (c) [2017] [Arif Nur Khoirudin]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
