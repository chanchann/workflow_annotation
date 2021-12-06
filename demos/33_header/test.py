import requests
 
if __name__ == "__main__":
    url = "http://127.0.0.1:8888/"
    
    resp = requests.get(url)

    print(resp.headers)
    print(resp.content)