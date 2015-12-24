#!/usr/bin/env python3

import sys, os
import subprocess
import http.client

argv = sys.argv

if len(argv) < 3:
    print("args: argv[1] = address/host, argv[2] = /path, argv[3] = pngfile")
    print("optional arg: argv[4] = formfilename")
    quit(1)

form_filename = ""

script_dir = sys.path[0]

template_path_1 = os.path.join(script_dir, "form_template-1.txt")
template_path_2 = os.path.join(script_dir, "form_template-2.txt")

try:
    form_filename = argv[4]
except IndexError:
    form_filename = argv[3]

address = argv[1]
post_path = argv[2]
png_file_path = argv[3]

form_p1 = open(template_path_1, "rb").read().decode("UTF-8")
form_p1 = form_p1.format(form_filename)
png = open(png_file_path, "rb").read()
form_p2 = open(template_path_2, "rb").read()

upload = b''.join([bytes(form_p1, encoding="UTF-8"), png, form_p2])
content_length = len(upload)

#print(upload)
#[print(int(x)) for x in upload]

myheaders = {"content-type": "multipart/form-data"}

conn = http.client.HTTPConnection(address)
conn.request("POST", post_path, body=upload, headers=myheaders)

res = conn.getresponse()

print(res.status, res.reason)
print(res.read())
