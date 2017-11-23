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
    users = BitVec("users_%d"% conflictedUniqueKeys, 16)
    groups = BitVec("groups_%d"% conflictedUniqueKeys, 16)
    time = Int("%s_%s" % ("time", conflictedUniqueKeys))
    conflictedPolicies[0]["type"] = Int("%s_%s_%s" % (conflictedPolicies[0]["type"], conflictedPolicies[0]["vendor"], conflictedUniqueKeys))
    listOfVariables = [users,groups,conflictedPolicies[0]["type"],time]
    for index in range(len(conflictedPolicies)):
	if conflictedPolicies[index]["rule"] == "comparator":
	    s.add(conflictedPolicies[0]["type"] > conflictedPolicies[index]["min"])
	    s.add(conflictedPolicies[0]["type"] < conflictedPolicies[index]["max"])	
	if conflictedPolicies[index]["time"] != None:
	    newDate = datetime.strptime(conflictedPolicies[index]["time"], '%Y-%m-%dT%H:%M:%S')
	    seconds = (newDate - datetime(1970,1,1)).total_seconds()
	    s.add(time > seconds)
	if conflictedPolicies[index]["user"] != None:
	    args =[]
            for user in conflictedPolicies[index]["user"]:
            	args.append(users == BitVecVal(user,16))
            s.add(Or(*args))
	if conflictedPolicies[index]["group"] != None:
	    args =[]
            for group in conflictedPolicies[index]["group"]:
            	args.append(groups == BitVecVal(group,16))
            s.add(Or(*args))
    return listOfVariables
	    

		 

#Parse the input XML file
tree = etree.parse('samplePolicyFile.xml')  
policyFile = tree.getroot()                         
parsedXMLFileDict = helper.parseXMLPolicyFile(policyFile)
#pprint.pprint(dict(parsedXMLFileDict))

'''Now we have our dictionary and we can check if a key has a list
with more than one value, which can then be fed into the SMT solver '''

#make constraints to give it to SMT solver      

conflictedUniqueKeys = 1
for myKey in parsedXMLFileDict.keys():
  if len(parsedXMLFileDict[myKey]) > 1:
    print("-"*20)
    print("There is a conflict in key: " + str(myKey))
    s = Optimize()
    listOfZ3Variables = addConstraintsInSolver(parsedXMLFileDict[myKey])
    print("-->" + str(s.check()) + "\n")
    '''if s.check() == sat:
        m = s.model()
	print(m)
        for var in listOfZ3Variables:
		s.add(var != m.eval(var))
	print(s)	
	print(s.check())
	print("*"*20)'''
    '''while s.check() == sat:
	m = s.model()
	print(m)
	for var in listOfZ3Variables:
		s.add(var != m.eval(var))'''
    if s.check() == sat:
	s.push()
	for var in listOfZ3Variables:

		if ("user" in str(var)) or ("groups" in str(var)):
			print("Satisfiable value of " + str(var) + " are:")
			while s.check() == sat:
				m = s.model()
				helper.printRequiredValue(var, m.eval(var))
				s.add(var != m.eval(var))
		else:
			v = s.minimize(var)
			if s.check() == sat:
				print("Satisfiable range of " + str(var) + " are:")
				if "time" in str(var):
					num = v.value()
					myStr= str(num)
					print(datetime.fromtimestamp(int(myStr)).strftime('%Y-%m-%dT%H:%M:%S'))
				else:
					helper.printRequiredValue(var, v.value())
				s.pop()  
				s.push()
				v2 = s.maximize(var)	
				if s.check() == sat:
					helper.printRequiredValue(var, v.value())				
		s.pop()  
		s.push()
		print("\n")
    	conflictedUniqueKeys += 1




  
  
