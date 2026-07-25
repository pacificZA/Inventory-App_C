# Inventory-app
a basic console app to manage inventories 
already existed in python but decied to remake it in C to learn how to code in this language,
besides that it's the closest possible from the python version: https://github.com/pacificZA/Inventory-app

## Command List
the action list is :
- help - Show this help message *
- new - Add a new item *
- edit - Edit an existing item tags *
- remove - Remove an existing item *
- exit - Exit the program
- +<number> <item_name> - Increase the quantity of an item
- -<number> <item_name> - Decrease the quantity of an item
- <item_name/tags> - Search for an item by name or tag
- all - Show all items
- delete - Delete the inventory file (cannot be undone)
- all command with an asterisk (*) can be used with the first letter only using a comma-separated list of values instead of interactive input, for example: n item_name,tags,quantity
