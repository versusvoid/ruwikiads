#!/usr/bin/env python3

import re
import gzip
import xml.etree.ElementTree as ET


link_annotation_regexp = '<a href="([^"]+)" class="listinglink">[^<]*</a>([^<]+)<'

id_key = '{http://www.w3.org/TR/RDF/}id'
resource_key = '{http://www.w3.org/TR/RDF/}resource'
links = set()
with gzip.open('content.rdf.u8.gz', 'r') as f:
    context = ET.iterparse(f, ['end'])
    #context = iter(context)
    event, root = next(context)
    for event, elem in context:
        if elem.tag == '{http://dmoz.org/rdf/}Topic':

            if 'World/Russian/Бизнес' in elem.attrib[id_key]:
                #print('business')

                for link_elem in elem:#.findall('{http://dmoz.org/rdf/}link'):
                    if not link_elem.tag.startswith('{http://dmoz.org/rdf/}link'): continue
                    links.add(link_elem.attrib[resource_key])

                #input()

        elif elem.tag == '{http://dmoz.org/rdf/}ExternalPage':
            topic = elem.find('{http://dmoz.org/rdf/}topic')
            assert topic != None
            if 'World/Russian/Бизнес' in topic.text:
                links.add(elem.attrib['about'])

with open('urls.txt', 'w') as f:
    for link in links:
        print(link, file=f)
