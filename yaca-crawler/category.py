
class Category(object):
    
    def __init__(self, path, name): 
        self.links_count = None
        self.links = set()
        self.subcategories = {}
        self.fullname = '/'.join((path, name))

    def __str__(self):
        return 'Category(links_count={},\n\tlinks={},\n\tsubcategories={}\n\tfullname={}\n)'.format(self.links_count, self.links, self.subcategories, self.fullname)

    def __repr__(self):
        return 'Category(links_count={},\n\tlinks={},\n\tsubcategories={}\n\tfullname={}\n)'.format(self.links_count, self.links, self.subcategories, self.fullname)
