import pycurl
import cStringIO
import json

import argparse

#HOOK_URL="http://hook.dooray.com/services/1387695619080878080/1688914151759469773/AOPFTLJxT3KkHLE1mG710g"
# LEGACY URL
HOOK_URL="https://hook.dooray.com/services/1387695619080878080/2125976240857721508/W9znGs_WSI6RXUad5PJhGA"
ICON_URL="https://search2.kakaocdn.net/argon/0x200_85_hr/LHQBRIQTTih" 
BOT_NAME="TCVMonitor"

response = cStringIO.StringIO()                                                                          
        
def generate_post_data(msg):
    fmt = json.dumps({"botName":BOT_NAME, "botIconImage":ICON_URL, "text": msg})                         
    return fmt

def send_data(url, post_data):
    ret = False
    curl = pycurl.Curl()
    curl.setopt(pycurl.URL, url)                                                                         
    curl.setopt(pycurl.HTTPHEADER, ['Content-Type: application/json'])                                   
    curl.setopt(pycurl.POSTFIELDS, post_data)                                                            
    curl.setopt(pycurl.WRITEFUNCTION, response.write)                                                    

    try:
        curl.perform()
        http_code = curl.getinfo(pycurl.HTTP_CODE)
        if http_code is 200:
            ret = True  
    except:
        print "Error"   

    curl.close()                                                                                         
    return ret

if __name__ == '__main__':
    import sys
    message = generate_post_data(sys.argv[1])
    send_data(HOOK_URL, message)
