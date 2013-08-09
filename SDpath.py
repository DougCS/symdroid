import os

def write_SDdata():
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

def read_SDdata():
    CONFIG_FILE='e:/symdroid/SDdata/SDsettings.txt'
    try:
        f=open(CONFIG_FILE,'rt')
        try:
            content = f.read()
            config=eval(content)
            f.close()
            value1=config.get('variable1','')
            value2=config.get('variable2','')
            print value1
            print value2
        except:
            print 'can not read file'
    except:
        print 'can not open file'

write_SDdata()

read_SDdata()
