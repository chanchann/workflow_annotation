## multipart/form-data

## some examples

```
POST / HTTP/1.1
Host: localhost:8000
User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux i686; rv:29.0) Gecko/20100101 Firefox/29.0
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
Accept-Language: en-US,en;q=0.5
Accept-Encoding: gzip, deflate
Cookie: __atuvc=34%7C7; permanent=0; _gitlab_session=226ad8a0be43681acf38c2fab9497240; __profilin=p%3Dt; request_method=GET
Connection: keep-alive
Content-Type: multipart/form-data; boundary=---------------------------9051914041544843365972754266
Content-Length: 554

-----------------------------9051914041544843365972754266
Content-Disposition: form-data; name="text"

text default
-----------------------------9051914041544843365972754266
Content-Disposition: form-data; name="file1"; filename="a.txt"
Content-Type: text/plain

Content of a.txt.

-----------------------------9051914041544843365972754266
Content-Disposition: form-data; name="file2"; filename="a.html"
Content-Type: text/html

<!DOCTYPE html><title>Content of a.html.</title>

-----------------------------9051914041544843365972754266--
```

```
POST / HTTP/1.1
HOST: host.example.com
Cookie: some_cookies...
Connection: Keep-Alive
Content-Type: multipart/form-data; boundary=12345

--12345
Content-Disposition: form-data; name="sometext"

some text that you wrote in your html form ...
--12345
Content-Disposition: form-data; name="name_of_post_request" filename="filename.xyz"

content of filename.xyz that you upload in your form with input[type=file]
--12345
Content-Disposition: form-data; name="image" filename="picture_of_sunset.jpg"

content of picture_of_sunset.jpg ...
--12345--
```

## https://www.ietf.org/rfc/rfc2388.txt

###  Definition of multipart/form-data

Each field has a **name**. Within a given form, the names are **unique**.

Each part is expected to contain a **content-disposition** header [RFC 2183] where the
disposition type is "form-data", 

and where the disposition contains an (additional) parameter of "name", where the value of that
parameter is the original field name in the form

```
Content-Disposition: form-data; name="user"
```

Each part has an optional "Content-Type", which defaults to text/plain.


todo : file : "application/octet-stream" and  "multipart/mixed"

Each part may be encoded and the "content-transfer-encoding" header
supplied if the value of that part does not conform to the default
encoding.

##  Use of multipart/form-data

### Boundary

a boundary is selected that does not occur in any of the data

## Sets of files

"multipart/mixed"

## Encoding

default：

7BIT encoding. 

"content-transfer-encoding"

## Other attributes

```
--AaB03x
content-disposition: form-data; name="field1"
content-type: text/plain;charset=windows-1250
content-transfer-encoding: quoted-printable
```

## Operability considerations

### Compression, encryption

### Other data encodings rather than multipart

The multipart/form-data encoding has a high overhead and performance
impact if there are many fields with short values. However, in
practice, for the forms in use, for example, in HTML, the average
overhead is not significant.

### Remote files with third-party transfer

### Non-ASCII field names

### Ordered fields and duplicated field name

## ref

https://www.ietf.org/rfc/rfc1867.txt