import os

def write_symdroidData():
    CONFIG_DIR='e:/symdroid/SDdata'
    CONFIG_FILE=os.path.join(CONFIG_DIR,'SDData.obb')
    if not os.path.isdir(CONFIG_DIR):
        os.makedirs(CONFIG_DIR)

def read_SDdata():
    CONFIG_FILE='e:/symdroid/SDdata/SDData.obb'
    try:
        f=open(CONFIG_FILE,'rt')
        try:
            content = f.read()
            config=eval(content)
            f.close()
        except:
            print 'Can not read file'
    except:
        print 'Can not open file'

write_symdroidData()

read_SDdata()
