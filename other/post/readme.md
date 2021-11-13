## post 

The HTTP POST method sends data to the server. The type of the body of the request is indicated by the Content-Type header.

## application/x-www-form-urlencoded

## multipart/form-data

Summary; if you have binary (non-alphanumeric) data (or a significantly sized payload) to transmit, use multipart/form-data. Otherwise, use application/x-www-form-urlencoded

the two Content-Type headers for HTTP POST requests that user-agents (browsers) must support. 

The purpose of both of those types of requests is to send a list of name/value pairs to the server

### boundary

**https://stackoverflow.com/questions/3508338/what-is-the-boundary-in-multipart-form-data**

In the HTTP header, I find that the Content-Type: multipart/form-data; boundary=???.

- Q : Is the ??? free to be defined by the user? 

YES

- Q : Or is it generated from the HTML? 

No. HTML has nothing to do with that. 

- Q : Is it possible for me to define the ??? = abcdefg?

YES

- Q : So how does the server know where a parameter value starts and ends when it receives an HTTP request using multipart/form-data?

Using the boundary, similar to &

```cpp
--XXX
Content-Disposition: form-data; name="name"

John
--XXX
Content-Disposition: form-data; name="age"

12
--XXX--  // (Yout have to add an extra "--" in the end of boundary)
```

**https://www.w3.org/TR/html401/interact/forms.html#h-17.13.4.2**

Note. Please consult [RFC2388] for additional information about file uploads, including backwards compatibility issues, the relationship between "multipart/form-data" and other content types, performance issues, etc.

As with all multipart MIME types, each part has an optional "Content-Type" header that defaults to "text/plain". User agents should supply the "Content-Type" header, accompanied by a "charset" parameter.

Each part is expected to contain:

1. a "Content-Disposition" header whose value is "form-data".

2. a name attribute specifying the control name of the corresponding control. Control names originally encoded in non-ASCII character sets may be encoded using the method outlined in [RFC2045].

**https://stackoverflow.com/questions/8659808/how-does-http-file-upload-work**

https://www.rfc-editor.org/rfc/rfc7578





## rfc

https://www.ietf.org/rfc/rfc1867.txt

## ref

https://stackoverflow.com/questions/4007969/application-x-www-form-urlencoded-or-multipart-form-data

https://imququ.com/post/four-ways-to-post-data-in-http.html

https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods/POST

https://www.ietf.org/rfc/rfc2388.txt