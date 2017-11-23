## This calls will parse the XML policy file and return it in a dictionary

#Some variables that we will use while parsing and creating our dictionary
parsedXMLFileDict = {}
ruleType = ""
ruleMin = 0
ruleMax = 0
description = ""
type = ""
vendor = ""
time = ""
user = []
group = []
globalUserDict = {}

def printRequiredValue(var, value):
	if ("user" in str(var)) or ("groups" in str(var)):
		for name, id in globalUserDict.iteritems():
    			if id == value:
        			print name
	else:
		print value
	

def parseXMLPolicyFile(policyFile):
  #Retrieve the elements from "policyFile" element and fill up our custom data structure
  #We are creating a dictionary where the rule, type and vendor will serve as a unique key 
  UID = 1
  valueDict = {}
  for policy in policyFile:
    for rule in policy:
      if rule.tag == "rule":
        ruleType = rule.text
        valueDict["rule"] = ruleType
        if len(rule.attrib.keys()) > 0:
          ruleMin = rule.attrib["min"]
          ruleMax = rule.attrib["max"]
          valueDict["min"] = ruleMin
          valueDict["max"] = ruleMax
      elif rule.tag == "desc":
        description = rule.text
        valueDict["description"] = description
      elif rule.tag == "attributes":
        for attribute in rule:
          if attribute.tag == "type":
            type = attribute.text
            valueDict["type"] = type
          elif attribute.tag == "vendor":
            vendor = attribute.text
            valueDict["vendor"] = vendor
          elif attribute.tag == "time":
            time = attribute.text
            valueDict["time"] = time
          elif attribute.tag == "user" and attribute.text:
            user = attribute.text.split(',')
	    userNewArray =[]
	    for u in user:
		if u not in globalUserDict.keys():
			globalUserDict[u] = UID
			userNewArray.append(UID)
			UID += 1
		else:
			userNewArray.append(globalUserDict[u])
            valueDict["user"] = userNewArray
          elif (attribute.tag == "group") and attribute.text:
            group = attribute.text.split(',')
	    groupNewArray =[]
	    for g in group:
		if g not in globalUserDict.keys():
			globalUserDict[g] = UID
			groupNewArray.append(UID)
			UID += 1
		else:
			groupNewArray.append(globalUserDict[g])

            valueDict["group"] = groupNewArray
    key = (ruleType, type, vendor)
    valueArray = []
    if key in parsedXMLFileDict.keys():
      valueArray = parsedXMLFileDict[key] 

    valueArray.append(valueDict)
    parsedXMLFileDict[key] = valueArray
    valueDict = {}
    key = ()
  return parsedXMLFileDict

