#!/bin/bash

previous_year=$1
new_year=$2

old_string="2018-${previous_year}"
new_string="2018-${new_year}"

echo ">>> Updating copyright notices from '${old_string}' to '${new_string}'"

sed -i -e "s/${old_string}/${new_string}/g" LICENSE
sed -i -e "s/${old_string}/${new_string}/g" LICENSE_HEADER
for f in $(find $PWD -name '*.cpp' -or -name '*.hpp'); do sed -i -e "s/${old_string}/${new_string}/g" $f ; done
for f in $(find $PWD -name '*.py' -or -name '*.rst'); do sed -i -e "s/${old_string}/${new_string}/g" $f ; done
for f in $(find $PWD -name '*.yml' -or -name '*.md'); do sed -i -e "s/${old_string}/${new_string}/g" $f ; done
for f in $(find $PWD -name 'Dockerfile.*'); do sed -i -e "s/${old_string}/${new_string}/g" $f ; done

echo ">>> Checking for occurrences not covered by automatic update"

for f in $(find $PWD -type f); do
    grep -H "${old_string}" $f >> old_notices.txt
done

num_remaining_old=$(cat old_notices.txt | wc -l)

if [ "${num_remaining_old}" != "0" ]; then
    echo ">>> Found ${num_remaining_old} old notices not covered by automatic update:"
    cat old_notices.txt
    echo ">>> Please provide to manual update or modify the script"
    rm old_notices.txt
    exit 1
else
    echo ">>> Update complete"
fi
