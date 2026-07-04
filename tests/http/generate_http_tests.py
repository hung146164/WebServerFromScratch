import sys
import os
import random
import string

def generate_heavy_http_request(output_dir, body_size_mb=10):
    print(f"[*] Generating {body_size_mb}MB HTTP request test case...")
    
    # 1. Generate heavy body
    # Using random printable ASCII to avoid any weird encoding issues while maintaining robustness
    body_data = ''.join(random.choices(string.ascii_letters + string.digits, k=body_size_mb * 1024 * 1024))
    body_bytes = body_data.encode('utf-8')
    body_len = len(body_bytes)
    
    # Calculate simple sum checksum
    checksum = sum(body_bytes) % 1000000007
    
    # 2. Modern and extreme headers
    method = "POST"
    url = "/api/upload/avatar?user=123&type=avatar"
    protocol = "HTTP/1.1"
    
    headers = [
        f"{method} {url} {protocol}",
        "Host: localhost:8081",
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        "Accept-Encoding: gzip, deflate, br",
        "Accept-Language: vi-VN,vi;q=0.9,en-US;q=0.6,en;q=0.5",
        "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c",
        "Content-Type: application/octet-stream",
        "X-Custom-Header-Very-Long: " + "A" * 1000, # Extreme header length
        "X-Empty-Header: ", # Empty header value edge case
        "X-Spaces-Header:     trimmed-value-with-spaces     ", # Spaced header value
        f"Content-Length: {body_len}",
        "", # End of headers
        ""
    ]
    
    header_raw = "\r\n".join(headers)
    header_bytes = header_raw.encode('utf-8')
    
    # 3. Write raw HTTP request to file
    request_path = os.path.join(output_dir, "heavy_http_request.raw")
    with open(request_path, "wb") as f:
        f.write(header_bytes)
        f.write(body_bytes)
        
    # 4. Write expected metadata for C++ assertion verification
    metadata_path = os.path.join(output_dir, "heavy_http_metadata.txt")
    with open(metadata_path, "w", encoding="utf-8") as f:
        f.write(f"method:{method}\n")
        f.write(f"url:{url}\n")
        f.write(f"protocol:{protocol}\n")
        f.write(f"body_length:{body_len}\n")
        f.write(f"checksum:{checksum}\n")
        
    print(f"[+] Successfully generated:")
    print(f"    - Request file: {request_path} ({os.path.getsize(request_path) / (1024*1024):.2f} MB)")
    print(f"    - Metadata file: {metadata_path}")

if __name__ == "__main__":
    size = 10
    if len(sys.argv) > 1:
        try:
            size = int(sys.argv[1])
        except ValueError:
            pass
            
    out_dir = os.path.dirname(os.path.abspath(__file__))
    generate_heavy_http_request(out_dir, size)
