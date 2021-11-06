## post 

The HTTP POST method sends data to the server. The type of the body of the request is indicated by the Content-Type header.

### application/x-www-form-urlencoded

### multipart/form-data

Summary; if you have binary (non-alphanumeric) data (or a significantly sized payload) to transmit, use multipart/form-data. Otherwise, use application/x-www-form-urlencoded

the two Content-Type headers for HTTP POST requests that user-agents (browsers) must support. 

The purpose of both of those types of requests is to send a list of name/value pairs to the server

## rfc

https://www.ietf.org/rfc/rfc1867.txt

## ref

https://stackoverflow.com/questions/4007969/application-x-www-form-urlencoded-or-multipart-form-data

https://imququ.com/post/four-ways-to-post-data-in-http.html

https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods/POST

https://www.ietf.org/rfc/rfc2388.txt