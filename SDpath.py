import os

class SymdroidData:
    CONFIG_DIR='e:/symdroid/SDdata'
    CONFIG_FILENAME='SDData.obb'
    def __init__(self):
        self.CONFIG_FILE=os.path.join(self.CONFIG_DIR,self.CONFIG_FILENAME)
        if not os.path.isdir(self.CONFIG_DIR):
            os.makedirs(self.CONFIG_DIR)
    def write(self, data):
        if type(data) is not dict:
            print 'Data not in the right format'
            return
        try:
            f=open(self.CONFIG_FILE,'wt')
            f.write(str(data))
            f.close()
        except IOError:
            print 'Cannot write to file'
    def read(self):
        try:
            f=open(self.CONFIG_FILE,'rt')
            try:
                content = f.read()
                config=eval(content)
                f.close()
                return config
            except:
                print 'Cannot read file'
        except IOError:
            print 'Cannot open file'


if __name__=="__main__":
    ### Testing ###
    sd=SymdroidData()
    print "Data path:", sd.CONFIG_FILE
    data= {
        'variable1' : 'value1',
        'variable2' : 'value2'
    }
    print 'Attempting to write data:',data
    sd.write(data)
    print 'Reading data'
    newdata=sd.read()
    if newdata != data:
        print 'Error, data missmatch'
    else:
        print 'Data success'
        print newdata.get('variable1')
        print newdata.get('variable2')
    print 'Finished'
