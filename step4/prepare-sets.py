#!/usr/bin/env python3

import random


def count_samples(file_name):
    count = 0
    with open(file_name, 'r') as f:
        for line in f:
            if line.startswith('---------===============---------===============---------'):
                count += 1

    return count

featured_samples_count = count_samples('../step2/data/output/featured-samples.txt')
wiki_ads_samples_count = count_samples('../step2/data/output/ads-samples.txt')
ads_samples_count = count_samples('../step3/data/output/yaca-ads-30.01.2016-08:44:27/yaca-ads.txt')

assert featured_samples_count - 2*ads_samples_count > wiki_ads_samples_count

featured_test_samples = set()
featured_test_samples_count = featured_samples_count - 2*ads_samples_count
while len(featured_test_samples) < featured_test_samples_count:
    featured_test_samples.add(random.randrange(0, featured_samples_count))

with open('data/output/test-set.txt', 'w') as of:
    with open('../step2/data/output/ads-samples.txt', 'r') as f:
        sample_text = []
        for line in f:
            if line.startswith('---------===============---------===============---------'):
                print(''.join(sample_text[1:]), file=of, end='')
                print('ads', file=of)
                sample_text = []
            else:
                sample_text.append(line)

    with open('../step2/data/output/featured-samples.txt', 'r') as f:
        sample_text = []
        sample_number = 0
        for line in f:
            if line.startswith('---------===============---------===============---------'):
                if sample_number in featured_test_samples:
                    print(''.join(sample_text), file=of, end='')
                    print('plain', file=of)
                sample_text = []
                sample_number += 1
            else:
                sample_text.append(line)


with open('data/output/train-set.txt', 'w') as of:
    with open('../step3/data/output/yaca-ads-30.01.2016-08:44:27/yaca-ads.txt', 'r') as f:
        sample_text = []
        for line in f:
            if line.startswith('---------===============---------===============---------'):
                print(''.join(sample_text[2:]), file=of, end='')
                print('ads', file=of)
                sample_text = []
            else:
                sample_text.append(line)

    with open('../step2/data/output/featured-samples.txt', 'r') as f:
        sample_text = []
        sample_number = 0
        for line in f:
            if line.startswith('---------===============---------===============---------'):
                if sample_number not in featured_test_samples:
                    print(''.join(sample_text), file=of, end='')
                    print('plain', file=of)
                sample_text = []
                sample_number += 1
            else:
                sample_text.append(line)
