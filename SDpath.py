import os

def write_SDdata():
    CONFIG_DIR='e:/symdroid/SDdata'
    CONFIG_FILE=os.path.join(CONFIG_DIR,'SDData.obb')
    if not os.path.isdir(CONFIG_DIR):
        os.makedirs(CONFIG_DIR)
        CONFIG_FILE=os.path.join(CONFIG_DIR,'SDData.obb')
    value1 = '.obb'
    config={}
    config['variable1']= value1
    f=open(CONFIG_FILE,'wt')
    f.write(repr(config))
    f.close()

def read_SDdata():
    CONFIG_FILE='e:/symdroid/SDdata/SDData.obb'
    try:
        f=open(CONFIG_FILE,'rt')
        try:
            content = f.read()
            config=eval(content)
            f.close()
            value1=config.get('variable1')
            print value1
            print value2
        except:
            print 'can not read file'
    except:
        print 'can not open file'

write_SDdata()

read_SDdata()
