def FindJUID(db, node_type, juid):
    matches = [x for x in db[node_type] if x['juid'] == juid]
    # at this point, "matches" is a list of dictionaries
    if len(matches) == 1:
        return matches[0]
    if len(matches) == 0:
        return None
    return matches

