from PYTHON_LIB.IMAGE_LIB.filter import FilterSynonyms

class PerSource:
    def __init__(self, db, set_ref):
        self.set = set_ref
        self.color = FilterSynonyms(set_ref['filter'],'CANONICAL')
