## This call is the main class and will take the XML policy file,
## use "helper" class to parse it and use Z3 solver to detect if 
## constraints are satisfiable or not.


# http://www.diveintopython3.net/xml.html

from z3 import *
import pprint
import xml.etree.ElementTree as etree    
from datetime import datetime
import helper
isBitVec = True

def addConstraintsInSolver(conflictedPolicies):
    '''user = BitVec("user", 16)
    users1 = BitVecVal(1,16)
    users2 = BitVecVal(2,16)
    users3 = BitVecVal(3,16)
    s.add(Distinct(user == users1 , user == users2))
    s.add(Distinct(user == users2 , user == users3))'''

    for index in range(len(conflictedPolicies)):
	conflictedPolicies[index]["type"] = Int("%s_%s" % (conflictedPolicies[index]["type"], conflictedPolicies[index]["vendor"]))
	time = Int("%s_%s" % ("time", conflictedUniqueKeys))
	if not isBitVec:
		users = Int("%s_%s" % ("users", conflictedUniqueKeys))
		groups = Int("%s_%s" % ("groups", conflictedUniqueKeys))
	else:
		users = BitVec("users", 16)
	
	if conflictedPolicies[index]["rule"] == "comparator":
	    s.add(conflictedPolicies[index]["type"] > conflictedPolicies[index]["min"])
	    s.add(conflictedPolicies[index]["type"] < conflictedPolicies[index]["max"])	
	if conflictedPolicies[index]["time"] != None:
	    newDate = datetime.strptime(conflictedPolicies[index]["time"], '%Y-%m-%dT%H:%M:%S')
	    seconds = (newDate - datetime(1970,1,1)).total_seconds()
	    s.add(time > seconds)
	if conflictedPolicies[index]["user"] != None:
	    if isBitVec:
	        for user in conflictedPolicies[index]["user"]:
			#users = BitVecVal(user,16)
	    concatStr = ""
	    for user in conflictedPolicies[index]["user"]:
		concatStr = concatStr + "users == " + str(user) + ","
	    #s.add(Distinct(users == {} % for i in conflictedPolicies[index]["user"]))
	    s.add(Distinct(BoolVal(concatStr)))
	    


#Parse the input XML file
tree = etree.parse('samplePolicyFile.xml')  
policyFile = tree.getroot()                         
parsedXMLFileDict = helper.parseXMLPolicyFile(policyFile)
pprint.pprint(dict(parsedXMLFileDict))

'''Now we have our dictionary and we can check if a key has a list
with more than one value, which can then be fed into the SMT solver '''

#make constraints to give it to SMT solver      

conflictedUniqueKeys = 1
for myKey in parsedXMLFileDict.keys():
  if len(parsedXMLFileDict[myKey]) > 1:
    print("There is a conflict in key: ")
    s = Solver()
    addConstraintsInSolver(parsedXMLFileDict[myKey])
    print(s)  
    print("-"*20)    
    print(s.check())
    print("-"*20)    
    if s.check() == sat:
        print(s.model())
    conflictedUniqueKeys += 1




  
  
