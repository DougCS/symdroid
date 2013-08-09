import os

def writesddata():
    CONFIG_DIR='e:/symdroid/SDdata'
    CONFIG_FILE=os.path.join(CONFIG_DIR,'SDsettings.txt')
    if not os.path.isdir(CONFIG_DIR):
        os.makedirs(CONFIG_DIR)
        CONFIG_FILE=os.path.join(CONFIG_DIR,'SDsettings.txt')
    value1 = 'man'
    value2 = 3.15
    config={}
    config['variable1']= value1
    config['variable2']= value2
    f=open(CONFIG_FILE,'wt')
    f.write(repr(config))
    f.close()
