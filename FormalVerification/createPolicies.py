# This script will create (n) conflicted policies, based on the below mentioned
# 3 base policies. It will output files sequentially.

   
import datetime 
import xml.dom.minidom
import random

policy1 = "<policy><rule min=\"%s\" max=\"%s\">comparator</rule><desc>in degrees Celsius</desc><attributes><type>temperature</type><vendor>Google</vendor><time>%s</time><user>%s</user><group>%s</group></attributes></policy>"


policy2 = "<policy><rule min=\"%s\" max=\"%s\">comparator</rule><desc>altitude in meters</desc><attributes><type>altitude</type><vendor>DJI</vendor><time>%s</time><user>%s</user><group>%s</group></attributes></policy>"

policy3 = "<policy><rule>access</rule><desc>Acess to car Fuel Level readings</desc><attributes><type>password</type><vendor>Latas</vendor><time>%s</time><user>%s</user><group>%s</group></attributes></policy>"


users = ["user1","user2", "user3","user4","user5","user6"]
groups= ["groupA" , "groupB" , "groupC" , "groupX", "groupY", "groupZ"]
numberOfPolicies = 10




def createConflictedPolicyFile(c_Policy_Number):
	time = startingTime
	mint = startingMin
	maxt = startingMax
	subUsers = []
	subGroups =[]
	policyContents = "<?xml version=\"1.0\" encoding=\"utf-8\"?><policyFile>"
	loops = c_Policy_Number/ 3 # 3 is the basic number of policies that we have
	while loops > 0:
		n = random.randint(3,len(users))
		while n > 0:
 			subUsers.append(users[random.randint(0,len(users)-1)])
		 	subGroups.append(groups[random.randint(0,len(groups)-1)])
			n -=1
		subUsers.append(users[2])
		subUsers.append(users[3])
		subGroups.append(groups[2])
		subGroups.append(groups[3])
		policyContents = policyContents + (policy1 % (mint,maxt,(time),",".join(set(subUsers)) , ",".join(set(subGroups))))
		policyContents = policyContents + (policy2 % (mint,maxt,time, ",".join(set(subUsers)),",".join(set(subGroups))))
		policyContents = policyContents + (policy3 % (time, ",".join(set(subUsers)) , ",".join(set(subGroups))))
		date = datetime.datetime.strptime ( time, '%Y-%m-%dT%H:%M:%S')
		newDate = date + datetime.timedelta(days=7)
		time = newDate.strftime('%Y-%m-%dT%H:%M:%S')
		mint +=1
		maxt -=1
		loops -= 1
		subUsers = []
		subGroups =[]
	remainingPolicies = c_Policy_Number%3
	n = random.randint(3,len(users))
	while n > 0:
 		subUsers.append(users[random.randint(0,len(users)-1)])
		subGroups.append(groups[random.randint(0,len(groups)-1)])
		n -=1
	subUsers.append(users[2])
	subUsers.append(users[3])
	subGroups.append(groups[2])
	subGroups.append(groups[3])
	if remainingPolicies > 0:
		policyContents = policyContents + (policy1 % (mint,maxt,(time),",".join(set(subUsers)),",".join(set(subGroups))))
		remainingPolicies -= 1

	if remainingPolicies > 0:
		policyContents = policyContents + (policy2 % (mint,maxt,(time), ",".join(set(subUsers)),",".join(set(subGroups))))
		remainingPolicies -= 1	

	if remainingPolicies > 0:
		policyContents = policyContents + (policy3 % (time, ",".join(set(subUsers)),",".join(set(subGroups))))
		remainingPolicies -= 1	
	
	policyContents +="</policyFile>"
	mxml = xml.dom.minidom.parseString(policyContents)
	pretty_xml_as_string = mxml.toprettyxml()
	f = open("policies/outputPolicy" + str(c_Policy_Number) + ".xml", "w")
	f.write(pretty_xml_as_string)
	f.close()
	print("-"* 20)
		


startingMin = -200
startingMax = 200
startingTime = "2006-07-16T23:30:30"
[createConflictedPolicyFile(current_Policy_Number) for current_Policy_Number in range(1,501)]
