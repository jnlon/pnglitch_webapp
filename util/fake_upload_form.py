#!/usr/bin/env python3

import sys, os
import subprocess

argv = sys.argv

if len(argv) < 3:
    print("args: argv[1] = executable, argv[2] = pngfile")
    print("optional arg: argv[3] = formfilename")
    quit(1)

form_filename = ""

script_dir = sys.path[0]

template_path_1 = os.path.join(script_dir, "form_template-1.txt")
template_path_2 = os.path.join(script_dir, "form_template-2.txt")

try:
    form_filename = argv[3]
except IndexError:
    form_filename = argv[2]

exec_path = argv[1]

form_p1 = open(template_path_1, "rb").read().decode("UTF-8")
form_p1 = form_p1.format(form_filename)
png = open(argv[2], "rb").read()
form_p2 = open(template_path_2, "rb").read()

upload = b''.join([bytes(form_p1, encoding="UTF-8"), png, form_p2])
content_length = len(upload)

#print(upload)
#[print(int(x)) for x in upload]
#p = subprocess.run([exec_path])

p = subprocess.Popen([exec_path], 
                bufsize=1,
                universal_newlines = False,
                env={"CONTENT_LENGTH": str(content_length)},
                stdout=subprocess.PIPE,
                stdin=subprocess.PIPE,
                stderr=subprocess.PIPE)

#out, err = p.communicate(upload)
#print(out, err)

wr_bufsize = 1024

i = 0
while i < len(upload):
    write_i = (i+wr_bufsize%len(upload))
    print("to_write:", write_i-i)
    p.stdin.write(upload[i:write_i])
    i += wr_bufsize

p.stdin.flush()
print("# fake_upload_form.py --> Check output directory!")
p.stdin.close()

print(p.stdout.read())

p.stdin.close()
#p.stdout.flush()
#p.stdout.flush()

#for i in p.stdout:
#    print(i)

    #print(p.poll())

#jout, err = p.communicate()
#jprint(out)
#jprint(err)

#p.stdin.write(upload)
#p.wait()

#print("png_length:", png_length)

#Popen()
