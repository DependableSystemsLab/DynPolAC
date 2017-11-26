## This call is the main class and will take the XML policy file,
## use "helper" class to parse it and use Z3 solver to detect if 
## constraints are satisfiable or not.


# http://www.diveintopython3.net/xml.html

from z3 import *
import pprint
import xml.etree.ElementTree as etree    
from datetime import datetime
import helper
import time
import os



class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

#s = Optimize()

def addConstraintsInSolver(conflictedPolicies, conflictedUniqueKeys, z3):
    users = BitVec("users_%d"% conflictedUniqueKeys, 16)
    groups = BitVec("groups_%d"% conflictedUniqueKeys, 16)
    time = Int("%s_%s" % ("time", conflictedUniqueKeys))
    conflictedPolicies[0]["type"] = Int("%s_%s_%s" % (conflictedPolicies[0]["type"], conflictedPolicies[0]["vendor"], conflictedUniqueKeys))
    listOfVariables = [users,groups,time]
    if conflictedPolicies[0]["rule"] == "comparator":
    	listOfVariables.append(conflictedPolicies[0]["type"])
    for index in range(len(conflictedPolicies)):
	if conflictedPolicies[index]["rule"] == "comparator":
		if "min" in conflictedPolicies[index].keys():
	    		z3.add(conflictedPolicies[0]["type"] > conflictedPolicies[index]["min"])
		if "max" in conflictedPolicies[index].keys():
	    		z3.add(conflictedPolicies[0]["type"] < conflictedPolicies[index]["max"])	
	if ("time" in conflictedPolicies[index].keys()) and conflictedPolicies[index]["time"] != None:
	    newDate = datetime.strptime(conflictedPolicies[index]["time"], '%Y-%m-%dT%H:%M:%S')
	    seconds = (newDate - datetime(1970,1,1)).total_seconds()
	    z3.add(time > seconds)
	if ("user" in conflictedPolicies[index].keys()) and conflictedPolicies[index]["user"] != None:
	    args =[]
            for user in conflictedPolicies[index]["user"]:
            	args.append(users == BitVecVal(user,16))
            z3.add(Or(*args))
	if ("group" in conflictedPolicies[index].keys()) and (conflictedPolicies[index]["group"] != None):
	    args =[]
            for group in conflictedPolicies[index]["group"]:
            	args.append(groups == BitVecVal(group,16))
            z3.add(Or(*args))
    return listOfVariables
	    

		 
def openAndParseXMLFile(filePath):
#Parse the input XML file
	tree = etree.parse(filePath)  
	policyFile = tree.getroot()                         
	parsedXMLFileDict, userDict = helper.parseXMLPolicyFile(policyFile)
	#pprint.pprint(dict(parsedXMLFileDict))
	return parsedXMLFileDict, userDict
	

def checkAndSolveConflicts(parsedXMLFileDict, userDict):
	#make constraints to give it to SMT solver  
	#global s 
	numberOfConflictedPolicies = 0   
	isThereAConflict = False
	conflictedUniqueKeys = 1
	start=time.time()
	z3 = Optimize()
	for myKey in parsedXMLFileDict.keys():
  		if len(parsedXMLFileDict[myKey]) > 1:
			numberOfConflictedPolicies += len(parsedXMLFileDict[myKey])
    			isThereAConflict = True
    			print("-"*20)
    			print("There is a conflict in key: " + str(myKey))
    			
    			listOfZ3Variables = addConstraintsInSolver(parsedXMLFileDict[myKey], conflictedUniqueKeys, z3)
    			print("-->" + str(z3.check()) + "\n")
    			if z3.check() == sat:
				z3.push()
				for var in listOfZ3Variables:

					if ("user" in str(var)) or ("groups" in str(var)):
						print("Satisfiable value of " + str(var) + " are:")
						while z3.check() == sat:
							m = z3.model()
							helper.printRequiredValue(var, m.eval(var),userDict)
							z3.add(var != m.eval(var))
					else:
						v = z3.minimize(var)
						if z3.check() == sat:
							print("Satisfiable range of " + str(var) + " are:")
							if "time" in str(var):
								num = v.value()
								myStr= str(num)
								print(datetime.fromtimestamp(int(myStr)).strftime('%Y-%m-%dT%H:%M:%S'))
							else:
								helper.printRequiredValue(var, v.value(),userDict)
							z3.pop()  
							z3.push()
							v2 = z3.maximize(var)	
							if z3.check() == sat:
								helper.printRequiredValue(var, v.value(),userDict)				
					z3.pop()  
					z3.push()
					print("\n")
    				conflictedUniqueKeys += 1
	del z3
	if isThereAConflict == False:
		print("There was no conflict in policies")
	else:
		runlength=time.time()-start
		return runlength , numberOfConflictedPolicies
	return 0, numberOfConflictedPolicies

allfiles = os.listdir("policies")
allfiles.sort(key=lambda f: int(filter(str.isdigit, f)))
for filename in allfiles:
	print(bcolors.FAIL + filename + bcolors.ENDC)
	xmlDict, userDict = openAndParseXMLFile("policies/" + filename)
	f = open("summary.csv" , "a")
	timeTaken , policies = checkAndSolveConflicts(xmlDict, userDict)
	if policies != 0:
		f.write(str(timeTaken) + "," + str(policies) + "\n")
	f.close()

