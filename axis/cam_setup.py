#!/usr/bin/env python3

import argparse
import urllib.request, urllib.parse
from sys import exit
from pprint import pprint


DEBUG = True

def debug(msg):
    global DEBUG
    if DEBUG:
        print(msg)

class CloudCam:
    NTP_SERVER = "time.nist.gov"
    
    def setup(self):
        parser = argparse.ArgumentParser(description='Configure Axis camera for cloudcam')

        parser.add_argument('address', type=str, help="Camera address")
        parser.add_argument('--username', type=str, default="root", help="Username")
        parser.add_argument('--password', type=str, help="Password")
    
        args = parser.parse_args()
        if not args.password:
            print("You probably need to specify a password with --password")

        # config CGI URL
        self.param_url = "http://{}/axis-cgi/param.cgi".format(args.address)

        # HTTP Digest auth setup
        passman = urllib.request.HTTPPasswordMgrWithDefaultRealm()
        passman.add_password(None, self.param_url, args.username, args.password)
        auth_handler = urllib.request.HTTPDigestAuthHandler(passman)
        opener = urllib.request.build_opener(auth_handler)
        urllib.request.install_opener(opener)

        self.setNTP()

        # write out hostname/pw to ACAP upload config file (if one doesn't exist already)
        try:
            with open(".eap-install.cfg", 'x') as f:
                f.write("axis_device_ip=%s\npassword=%s" % (args.address, args.password))
                f.close()
        except FileExistsError as e:
            pass
                
        
    def request(self, data={}):
        r = None
        try:
            r = urllib.request.urlopen(self.param_url, urllib.parse.urlencode(data).encode('utf8'))
        except urllib.error.HTTPError as e:
            if e.code == 401:
                print("Invalid username/password")
                exit(1)
            else:
                # bluhh?
                raise(e)

        data = r.read()
        r.close()
        return data.decode('ascii')

    def setNTP(self):
        # attempt to fetch config
        
        fetch_config_params = {"action":"list", "group":"Time.NTP.Server", "responseformat":"rfc"}
        dec = urllib.parse.parse_qs(self.request(fetch_config_params))
        if not 'Time.NTP.Server' in dec:
            print("Did not get expected configuration in response. Response:", dec)
            exit(1)

        server = dec['Time.NTP.Server'][0].strip()
        if server == self.NTP_SERVER:
            debug("Setting NTP server...")
            params = {"action":"update", "Time.NTP.Server":self.NTP_SERVER}
            res = self.request(params)
            if res == 'OK':
                debug("Set NTP server")
            else:
                print("Failed to set NTP server: ", data)

cc = CloudCam()
cc.setup()
