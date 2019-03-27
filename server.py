import sys

class Stack:
	"A container with a last-in-first-out (LIFO) queuing policy."
	def __init__(self):
		self.list = []

	def push(self,item):
		"Push 'item' onto the stack"
		self.list.append(item)

	def pop(self):
		"Pop the most recently pushed item from the stack"
		return self.list.pop()

	def isEmpty(self):
		"Returns true if the stack is empty"
		return len(self.list) == 0

def read(file_name):
	to_ret = []	
	myFile = open(file_name)
	to_ret = myFile.readlines()
	to_ret = [line.split('\n')[0].split('\t') for line in to_ret]
	if len(to_ret[0]) == 1:
		to_ret = [line[0].split(',') for line in to_ret]
	for list in to_ret:
		for i in range(len(list)):
			if not list[i].isalpha():
				if list[i].isnumeric():
						list[i] = int(list[i])
				else:
					try:
						list[i] = float(list[i])
					except ValueError:
						pass

	return to_ret
	
def unique_counts(part, column = -1):
	result = {}
	for list in part:
		if list[column] not in result:
			result[list[column]] = 1			
		else:
			result[list[column]] += 1
	return result

def gini_impurity(part):
	total = len(part)
	results = unique_counts(part)
	imp = 0
	for elems in results:
		imp = imp + (results[elems]/total)**2
	return 1 - imp

def entropy(part):
	from math import log2
	total = len(part)
	results = unique_counts(part)
	imp = 0.0
	for elems in results:
		imp = imp + ((results[elems]/total) * log2((results[elems]/total)))
	return - imp

def divideset(part, column, value):
	split_function = None
	if isinstance(value, int) or isinstance(value, float):
		split_function = lambda prot: prot[column]>=value
	else:
		split_function = lambda prot: prot[column]==value #defineixes una funció interna a la qual quan li passes una fila buscarà la columna indicada i retornarà true o false segons el valor que es #funció = lambda <params>: tractament de params. i se li passar del pal funció(params)
		
	set1=[]
	set2=[]
	for list in part:
		if split_function(list):
			set1.append(list)
		else:
			set2.append(list)

	return (set1, set2)

class decisionnode:
	def __init__(self,col=-1,value=None,results=None,tb=None,fb=None):
		self.col=col
		self.value=value
		self.results=results
		self.tb=tb
		self.fb=fb
		
def buildtree(part, scoref=entropy, beta=0):
	if len(part)==0: return decisionnode()
	#Set up some variables to track the best criteria
	best_gain = 0
	best_criteria = None #(column, value)
	best_sets = None #(true_set, false_set)
	
	for i in range (len(part[0])-1):
		dictionary = unique_counts(part, i)
		for value in dictionary:
			div_setL, div_setR = divideset(part, i, value)
			calcL = scoref(div_setL)
			calcR = scoref(div_setR)
			delta = scoref(part) - (((len(div_setL)/len(part))*calcL) + ((len(div_setR)/len(part))*calcR))
			if delta > best_gain:
				best_gain = delta
				best_criteria = [i, value]
				best_sets = [div_setL, div_setR]
				best_results = dictionary
		
	if best_gain > beta:
		leftnode = buildtree(best_sets[0], scoref, beta)
		rightnode = buildtree(best_sets[1], scoref, beta)
		return decisionnode(best_criteria[0], best_criteria[1], None, tb=leftnode, fb=rightnode)
	else:
		return decisionnode(results = unique_counts(part))

def buildtree_ite(part, scoref=entropy, beta=0):
	if len(part)==0: return decisionnode()
	queue = Stack()
	queue1 = Stack()
	root = decisionnode()
	
	queue.push(part)
	queue1.push(root)
	while True:
		if queue.isEmpty():
			break
		else:
			part = queue.pop()
			node = queue1.pop()
			best_gain = 0
			best_criteria = None #(column, value)
			best_sets = None #(true_set, false_set)
			for i in range (len(part[0])-1):
				dictionary = unique_counts(part, i)
				for value in dictionary:
					div_setL, div_setR = divideset(part, i, value)
					calcL = scoref(div_setL)
					calcR = scoref(div_setR)
					delta = scoref(part) - (((len(div_setL)/len(part))*calcL) + ((len(div_setR)/len(part))*calcR))
					if delta > best_gain:
						best_gain = delta
						best_criteria = [i, value]
						best_sets = [div_setL, div_setR]
						best_results = dictionary

			if best_gain > beta:
				node.col = best_criteria[0]
				node.value = best_criteria[1]
				node.results = None
				leftnode = decisionnode()
				queue.push(best_sets[0])
				queue1.push(leftnode)
				rightnode = decisionnode()
				queue.push(best_sets[1])
				queue1.push(rightnode)
				node.tb = leftnode
				node.fb = rightnode
			else:
				node.results = unique_counts(part)

	return root

def classify(obj,tree):
	if tree.results!=None:
		return tree.results
	else:
		v=obj[tree.col]
		branch=None
		if isinstance(v, int) or isinstance(v, float):
			if v>=tree.value:
				branch=tree.tb
			else: 
				branch=tree.fb
		else:
			if v==tree.value: 
				branch=tree.tb
			else: 
				branch=tree.fb

		return classify(obj,branch)

def test_performance(testset, trainingset):
	tree = buildtree(trainingset, entropy, 0)
	print('\n----------TRAINING TREE:----------------')
	printtree(tree)
	print('\n')
	correct=0
	false=0
	for set in testset:
		a=classify(set, tree)
		print('CLASSIFY: ', set , ' -> ' , a)
		for key, _ in a.items():
			if set[-1] == key:
				correct += 1
			else:
				false += 1
	correct_percentage = correct / (correct + false)
	print('El nombre d\'encerts es:', correct, '-> ', correct_percentage*100, '%')

def prune(tree, threshold):
	from math import log2

	if tree.tb.results is None:
		tbranch = prune(tree.tb, threshold)
		if tbranch.results is None:
			return tree
	else:
		tbranch = tree.tb

	if tree.fb.results is None:
		fbranch = prune(tree.fb, threshold)
		if fbranch.results is None:
			return tree
	else:
		fbranch = tree.fb

	new_results = {}
	t_results = {}
	for key in tbranch.results:
		t_results[key] = tbranch.results[key]
		if key in fbranch.results:
			t_results[key] = tbranch.results[key] + fbranch.results[key]
	#unió diccionaris
	new_results = {**fbranch.results, **t_results}

	total_elems = 0
	for key in new_results:
		total_elems += new_results[key]

	imp = 0.0
	for elems in new_results:
		imp = imp + ((new_results[elems]/total_elems) * log2((new_results[elems]/total_elems)))

	if (-imp) <= threshold:
		tree.tb = None
		tree.fb = None
		tree.col = -1
		tree.value = None
		tree.results = new_results
	
	return tree


def divide(data, percentage = 100):
	testset = []
	trainingset = []
	size = (percentage * len(data)) / 100
	for i in range(len(data)):
		if i <= size:
			trainingset.append(data[i])
		else:
			testset.append(data[i])

	return testset, trainingset

def printtree(tree,indent=''):
	if tree.results != None:
		print(indent + str(tree.results))
	else:
		print(indent + str(tree.col) + ':' + str(tree.value) + '? ')

		print(indent + 'T->')
		printtree(tree.tb,indent + '  ')
		print(indent + 'F->')
		printtree(tree.fb,indent + '  ')



if __name__ == '__main__':
	data = read(sys.argv[1])
	print('-----------------ARBRE RECURSIU-----------------')
	tree = buildtree(data, entropy, 0)
	printtree(tree)

	print('-----------------ARBRE ITERATIU-----------------')
	it_tree = buildtree_ite(data, entropy, 0)
	printtree(it_tree)

	print('---------------------PRUNE---------------------')
	pruned_tree = prune(tree, 1.5)
	printtree(pruned_tree)

	testset, trainingset = divide(data, 10)
	test_performance(testset, trainingset)



                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          #����I설H8ԚC�´��1����#i!�`icTHBd�
D *\����= ^�Լ��D ���( �������e��ښ�oZD^z �F��)��鳽��/̽5g�90J$�Q%X�T���TU��@QaZA`��B:@Q�;EB��TSfj���#��(W7_������ #�!��+B��9X�`�cAzXG�P��w[|����ҮWE'�9����$���'�q@n�r�ƪ�kV�d�w%�~�j�"�h������j�R�jAF�RM-���N��'�,!`�WAK�� ����ΰ 	�ҥGJX@3�@V6 n���^�:` h�'�����spwˏ���v@�TV:���j��8�[���Y��
؜$�
A0"
�B魇�������?$��o�E��#�����r���`�؏yl@8!�%@�AE��8��v�v.�є~��^D�HQV���p	�ك�I�zlx�����8�6�/c^��h�1Ŕv&���ćZI!�Ƚ'^����4
p��hN�$�M�i�P&} n Hj:\\ D�x�UP3C�lD��@�T�B�y�@ ������2�� �`aY.\���@�ͺ���⮜�E�������͍C���8)2�2%a�����!r!����@87�h�����QB6I�7��������  p!�=*�2Gl��P \��6F-(�{ �y��Uz�q[�ډ���@�`8R��Vs$�{��
 ѹ����HV0N�L��g�ͫ��-T���}+K#cr+Nm�!D�J@k0��k �a@�G2�L �hUZ��!� !(J{�]�f��� ��T��f:n�Ir�	賩i�T�I��lʾ��8��ڦi���*4N�������cɯ�:鸄�5"�bJAF��� %UnD^$��[�*�$�#������Q/>�w��Ϳ�_�Q_���R��6 �!�E2��C�N��T������l��\��>��$ذj�0W�Wa"엊�H���O������|<��k�>��<7M�?�pR{V�uӭ��:c*�1JEr 4sP��G�@���2�.!9��*S�H L����9i�]���i���)���-�RJΌw���A�9}M����Ix%<!���9��ߵe��"�$ ���DI�9,���P"	����w��*�r�)�S��� 28  �c�˥N�j۸w�g!�TS�����N��[�xO��T���'� �i�T�e��3��4�)-�}�W8�{��ԧ��?+<��X�#ţ����q��D3yO�{�!�AnSB7A��/�����$����5L��!P6x�Լ����ko'��e�nHY���|��x9;L%S X�|��؟W<����G;4�0O�ؑ���fV*Bop *�
�2����I�-�1j �w�Z�'s���X�Ť���ʖDb�����e�|�J�]Nr�N�-��]-Fk!O�m��/z�Hұ� ]۩���KcI���(�'g9������kb�;�o����z0"Òh�6�BL��)Ċ,̓�4�L�3\	pY-�&�+R������͑�2w��/�1���v�z_�+�t-+��i�us"�>���v��NȵEN�[�[�gyK��di5iP�c������՚aOqH#�b�U��	�Ѓ+��K��@�N#D�l-�_��Ho�D��:k��x=]֖.�@� �Q�W��vseqyD3�IM���h���MP��g����ɻ��Y� ��z�מ/��}�8j���4   !�hL�|�X֪���׎�Ϡ�k��~�pt~��3c�s'%�?fw���FCWD�|A�~�	�g٭r�xt}�֌'BR�w\�m�)�&���/6���3�R��ه�F�E��U�jJ���݉�u	�WkW~"3�������~	"�̓f�o�����6x�dFN��N}c����A�/]0Y�-E��VN��b��y���^�������g��
3k�1��;���˿0F��=X�j|f�XSx��#�_y.�6�;7J#{
�%��~����?oҁ7c7��H��lfM��2F;�(�E�±F��Ci3���cU���e���&k���AT��o�f��;���e�.RRGsfu��hQ�[v�u|�d�UI~E��G��L��Io�7�V��L*�29m`9�u%d ��!܀    �  J�A�f��-~L� K(S�R�P�_żY�\� n�P��YR)gv����i9�%i����ƶT���6�V����] �ҧ��@�K󀾢R)�e�5�uE49�#0�w���Sepr��NG����0�蛸G��ǰ��x�.��>t��iFM�H��S<|�7l����v.����6�;t[�u1����&7f��ug��񭇞�P=)s�ԾL��2�s�L�k��6��+�+�����[�5Qcz^����w�ń�ԝ��X���!��m@�̓��3�{j�م���8��*]��=�S��\��G)�?(�
~/��쪮�z�L�8�l���QIE��uZ"̼�vCz������ b|�������J�,�����DY���`������@�U[�=R��Ic�B��\�����K�r�S�d����i���(~y���5����������}���ԌV������g	h��j�<��&A9���� ���Pn�������\�*�^��l��d�_:D�l廃��=)L:����C�+������LU8��f�i�s�1��T�e�;�Ƨ3�^c���Sx2+s�8B����na�|�C}����ơ��L����LK���3bI+��ߦ�s���E$�&O�P��yf7�a�Wjk@����a�_�U/��2�V������|v�I9I�լy�m
��k��_b���+�wN_�*t���,�\���>.2z�ܞ�Y�,,�Y�1�I^�|>xy �3��Y>^�yWxɅW���1��F:�IQ= �WV�V`�~b-J���G�W�*�= �M�S_=��X̌by��{QsCd%��4��ɽ���� b`���1MϜw�2Ä��
��8@ᷭ Wlsp�P�!.��L���VWfp�.�έL!��7������L\~�s�ir�.v��q�	x	j`�M��ߎzl01��ju�L��O��	�9{�	���%Y��Bj�f��'v�<p^���CڠK*��w~Zr���*��׺�0��$��c�� )]C`.nl�M��5��گo$��s�ch�4oM^����V�-����S����P����eq�����#Ît��`�e�B��h莈��T��l�Ib���,�秽M�W貪�hrlfЍU�׀v�:����&���wK]'7H�O����z\-�FN0����7��ￗ�_M���PI���O�r��/:n����k!�Y���DFt�!�H`:;;t�o\ELd����Zs���������C���"�w3����ٸ���w�gb2��)���wNY&��P�X���p3��aS/��N�5,��>�l �}L�HhC��UG�M�6L�8gyfr�Nٙ��Z ���m��mu�b���ґ�1�@~����A��nI�e����F~!\&��iy��5F��@Jk���|����ϻ�z����Ԇ��bz���YʸG�bB�<7^�@���u2/�L��&���.:T�Ϟ���[�
��MRW�qN@�(����ј����ǀ�dG��1g�0aJ��Z�f��"�lwl���5��y2�J\��@�	��-��`,W
��?
�~eX�����仾�������Н��	9�0�M�Rso�:X6>�s�>T��"�%�k��d��;~��R"���8ꀑ��Jt�	b~��l��)ڢ���:RJ�{�i�1�r�_�upl�"C�z�I�x�1_Uc鿾����=�����g!�gE?�/�nQ��϶�j����#Df��#ru&|��������K���ƻ�[�����"���9*�b\n�[��&:�l,��/�\��=wh��x�4�������>W_|�1�FD#�[]��v@�G�X�o�����mb�M}N��b-�Z5�ْ�ʈ�J�7�51�� zm���$��",>Y�9�8�u]�F:�� �ӮPA�QƟ��s9�c�bL��k_h�U b�mB�xr����?>K�-�e� ]8 [7�f_���A�}K�&஋��s��E(doJ��AL�:'��"�I��'��2��D����ڑ�	ڱ
�Mz"��y�t�󞮠:$�]/��*$���go.��5X�>^�oD(�U�d�T*�($�2!?� ]tJ����s���SPvbm�<���i��dm���v	�� ���|%^u�4xW ��V �(l�{T4�_����ӱ�Rc˅�\>�����=YFH��[L��<;�<��+�`,�j�.j��i�����$�)�Rq�M{q�|���m��{z����*T��w�-�E ���6���m����������Xk���3!�xS�-��}=6.o%9T������6����NV�3Ѵ>��6��\�Yv�\eU�t�d
]`3|�>�
,��FZa5�!{#�gLWˠu<�dp(,hZ�[�}��"K���3P]��r#Mh�0:MBQ�L��$v<K^���39U3�>��\悤��������,�yH�� �W��$�����A��z�ǑB��Z]��+�]�}�v�����_^y9��!�¾�,9������a;���Pb�ܕS	&��W�E��mN\��%�8�y� ���0kEW�h��H�G�$�[PioJ��
6�8���( <%A/�����Ke��ǈs�Y�����1.u�3e-yƝ�u�Ħ���!�����6�B:,�~��卙|�N�S���Ş�"U�д_���®ԅW���#�|�<��K��A�d���V?y`j��<�M9'��ܷ�C0��iŹ7�&<f׬>T�ϟ�m'��8�j�<B�{��X�c�K%��/Sd�!Ƥ+|�:%6�(�1W���<s��z*QӨ��V��1���~R��=���
�INc�2�Еƍq���	P!��8�$>��iR0��*	J:eZN�
�&~ʧ7 ln��#& �[f�oA�R_�Q.�$��v�G�)-ҾM�}S�2N���>
���ٵAQMH��1�d:G���������j�h��]�QEd3����M�܎~�F�?S0�@�&Jex��b1ݩ��a0�+�%�qi�0'X�Ek)iܰ�R��C�~U'�$�h�:oZκs�l,+ �Y�3̺B����{ ��,Ӌ�F���т�?��kJ���V�<����ԉAռ�ߤ#e(F��/���B���y�#?����J>�����r� ْĂ\|�I��t��'n�
�7C��7�WM��{ ��QnUM�ܰ]A�+*�e��|ۊ@4{xd�;��C�|-Ϻ�+���a��g`ǖTA�{6H�2�q������y���2�~nGqP�9����W��,YV�LfHlQ�U�H^��6��G҂���$�a���k�������$�p*���鹑禺H��R�$��}������|� 1�ue�k�\C3�qU������>X+/�&}�҈W�=h�=҆���$�)�� �I͋&���7���W�o�����w���J������6n�C����2���H�q�l0u�(�<�P��
�4C���y���:M��-j��GT馩���F�|������3��:,����ҭe���#w��Aٲf)��h�Z����μz�!��/QTl��7�`!kh1�I��7���G�A��IHE�b��3oj�pss�u�P��S2S�pۂr�D�
f4¶�W,����ҹJ�Ս��A
����	���1k����0�Z�^-~;�r�ɇ��Nl����mS��6�uR
����)�n��X�?���tz#�a��*��֏�Ŀ��L�����,/=��o��Ч���_�ݤ*�@5<:�l1��%m��D����ז�)gx(�>��2^V>ʧV^6�J3x���^���ר#b|�!�6n���gǢ�����Gb><>f��s
�	6>@��h���W�� 2�m����:���RyL�����(���0Zד��D�/ �����cKvo�|��]rJV�x�,���*]8#Z��V���-dpH���+?�r�)�<�o[�=��}��1^�\\쮻p�A1��e��l�1��,il"�g(N��S�l�T�h��J%o"����ħHA�;��}yzA�������ֱ�Q,�����*\�� �x|]m�I"���,���үқg��A�Ŵ�d�V{o43��h2V=��=k�e�b��aZgˣ�3n�lo-У?#�t�%9|����U(˓�	� Q��(�%��Ѽ(����%��z�B��JB�����³�{��h�\'���fj�!�4ʗ�s��Uj��_��:<}�/�͹�4Q	����7wÓH�⍜Q�r��ti{��c_漃�_�<d�1���[K�4    LN�v�p��u����y���>yS�ǀ���~��Vsd  
���K
=�R���+�$[5pE�fy��"@%dRk�i���Ԑ���A�W8�pnci���v�?�3���7���A@݌~9�tK����������p���6�c��o��	��x
��0q�����ʢr��́�?�"T��x�����ӛ�+� ��7����P\�R�f��k��.�m�-��N]?�Yy�V��	�3��߻�h�V��o'���PV��������O,@-�)uB9E?I�����FEB�r4'	y8�2}`�~#-�p_�c��4�"�"��ݨ���2���Ǩ{���˚��A���,�h����}����>��ysyVL����Pv�A�ʅ�W�֎�U`铑O�V&��h��kF���^��iT@�6Ra�al$%͜&(���7*��  '��o��j��^����MIm�:�����#������������$�#�f��<	uiYV����?	�=��ΗE�cR�����L��\7K�H�Zz`�����l�C�g"Z��f��fj~v|��c������9d�Hn�i�~�B�<_����7*nA��B����4N�h�����:h9@��pQ�g�r���:�gʧ<�a���U+��r �c�b�)�gu�,+zf�h��c@�ͅ�Z��9L}P�΀���7U/6��/ty�q���ge�� &��:�$�A@F�Q�9�q�&Zlm�t�i�rk×M�|m,T�7��x�4��&�L�'X�m(�<��6<��,
s'+�ݼМd��\B
n�}�/z P��>�yZ���]���Z�!N��&?ۼɍ��]u��	����ScUf9�2ޅ�~���Qȼq�68Y��RK�6�Ǡ~�T<�q� M�����a���|��ʘ��4}mI�߇�-Q3k����� cj3"���a����^�}�ŇP� ve�y����������ۺ��N�L�3�Z��S���7���h��E�{�fW�GHWv.F�Íq�������H�)c_�����}Ə����`��D�z���(����Ȭ�$Od���~�Da���4T�L�b�[Ǜcp�ç�(�)�<�E2ΉP�ã����v�Y<LKR��=�{�jP��#�6t,JMa����|��,���F,.�xtPrOGlܢ���s�;����Me)��`�;�$RY��5���R(���1��p�٥T�T��T�% �ot0s����v��&�y���w%���Mu�6lg9�D��=���!���c��Ԫ&�lÆm�k����I67��Y-�'��	���B����������D� ,Y�I�?����]�U��޲���a�s�҉l{��1�m��
]j��`|�R��3γˣ!�8�ď;b� u�ȿpe�֎�˪�j�.��+&C�uO� {��	�ѝ"ty�Ǚ��=R'���$�8:��@HW�P:�H��̌��Dc�@?�d��!�\]�ML<�l�I��qWI����qO �n�<����ɠ檏d��&����c?�3�b��<�p��~ZQ))6�S�|e�������v�\��q�0#Cj�LZRQ��K��a��G����n ��mwa��~��V	B
���L�H�8�(�%ou ዢiZ�H*a~V[���H��U�ف�z�j|�H�f&�i�&e��`ԧ�\� ��"�#�J����V7w�˼�t�>z��E*W�"<�s���O���G���w	�L?O
%�գƦ�0���H2��Ol:���A ��x��#�+����q�L{.6�ha�a�:4�*�yx��/�4�� /�q�ф�f�p��w�q���g^i��k�;W���i���M�|��:�_���t�=2~��HO�X8��~�n��ƊE��-{o%"8�3�ɡ ������:��~e>b|�:K^m���&m�|�#AZ�+f	�߳�����u�R򯗙PH��g�xF�'��6�-��Pc�+.Q��y]pA�ON_���Ө,�&�Q�!�e�%��Kk�����DM���:)���q�d�Ά����k8��"E�Q�/J�&3�	J�f��5PJ)����p�,,�E�^E� ҼO0c�@��� e<��C��Vm����,���˴�9<8�e���^������v�C��-���8<��o�uk�?�.1�"�1��Af�N����4�J-��>���c$_S��S�N����'�Zbҭ݂��d�T���߱��ҿ��S���Z��+�(�j��]�g2KޏUg� �cE�7�]F|>Q:7y�� �TD�q?Q?�0")��	{Ft�"}���J�M�V��ܴ}�hb��З���&!NNr2I80��k�+�%���w��W��%F��âo��0�;����_��˦��/���NIh1�����6ѹ{ps�\h:S�Y��^6�)�K,�gjܐ�FT���֮VVDg	�tž��y���/�g61������;lԹb�tlK�G6�[U�5����`�d�U{�M+Hs�n�N#�Qpgv��Y����Y��h�睤�I�ËI��_�JP�N�k+��, �g�]7���3����D3g��p�Ŏ��r����آ�S|-ܸ^)XO��z�0�+�W(+JʏGSSb5r��� l&ᐑv?ıp���$������w���zD�ט��[�]vB��6Ũ��q�
��Gi(�F��zx᩵2�LTj���0�d5'V���34p8XZ���nIPY�����V�	��<����SǍg@h>>�ܷﺢS�:;UCVn�[��"7�o��S��� P��m�5 Av%+A��7���Aԏ%-��mi
۽S#�R!p5�����aw��q����,O��O�ü��M�oVi�[zo9\8^N�F'M6�8��f�>O��ŗ�9�C�#���ɧ�H�*�ݟއ��z(i��z��noP����I��7~0NGa��"���q>�(2�HYc��z�b�!�&�+�v�pL��[0&{#<Y?c�~Z�ؽ����홹�ٗ�z.I����������%�B�y���W8����1��:�<��\���t�7Sϼ:�#��X�\:N�iPsV7���zX�{u�c]1��t-�_r�h��%�o�>]U0�����Yf(�L�D�W`Nf2��Rp�¤e���E��~��߾�/:�/�I��ބ��ט�Qe�g,wbV���x�h?榨n`�X8���:ӷDL����o����Y��nSM��!rޠP�ޖ8I��\ �OI�ڒ�g}�����d�R��F������s�.� ����ڧ�����职�~�4O.�ᘇK0��R��0�=h�^�	�gy��N����(O���}H��.M�=yc�� ���;ʏAw������2/��u�]t�$Ό���΢���u�%�6i�(�ҁ�^�tsC#t�,`\m:Q&�ǁ{!�����o�c�k�2^U g6)Bz�kJ&D-���:��as��5z^b%�Q�Nv���il(V���g;s*PlmhH����LV�K������N�o�u~�c'�-�S�R���;,b���XOn��+E�o)�8�.�Ρˌի65Y�9�)dS�+���un�`��'��	�pA�� �XB�]�ؚ���6%#M���:�M�:�s�&5���,0��V^��*>�:q��@���ǣ�LD�s����,&hЦR0 ��!$�7ќLo�m�����R��@��':��lǑf�T��~�W&>?ϝ&��-!"3+���:��[�<~r���������&��
�,M5��]����a��+4�{R`�u)ܣ�.)�9�y���^]a`'�߬||@�{%��EUc��
j%0��7�}���W�Cn��ZQ��gp�ߛ�