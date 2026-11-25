import time

import requests
from hexss.pyconfig import Config
from hexss import keyboard
from sklearn.ensemble import RandomForestClassifier

clf = None
c = Config()
if c.blue5 is None:
    c.blue5 = []
if c.white5 is None:
    c.white5 = []
if c.white2 is None:
    c.white2 = []
if c.mix is None:
    c.mix = []


def req():
    try:
        d = requests.get("http://10.87.1.90/api/data", timeout=2).json()
        s1, s2 = d["sensors"][0]['raw'], d["sensors"][1]['raw']
        return (s1['r'], s1['g'], s1['b'], s1['c'], s2['r'], s2['g'], s2['b'], s2['c'])
    except:
        print('req error, try again')
        return req()


def fit():
    global clf
    X = []
    Y = []
    for rgbc in c.blue5:
        X.append(rgbc)
        Y.append('blue5')
    for rgbc in c.white5:
        X.append(rgbc)
        Y.append('white5')
    for rgbc in c.white2:
        X.append(rgbc)
        Y.append('white2')
    for rgbc in c.mix:
        X.append(rgbc)
        Y.append('mix')

    clf = RandomForestClassifier(n_estimators=100, random_state=42)
    clf.fit(X, Y)
    print('\nfit ok')


def predict():
    global clf
    if clf is None:
        fit()

    probs = clf.predict_proba([req()])[0]  # ได้ array ความน่าจะเป็นของทุกคลาส
    idx = probs.argmax()                   # หาตำแหน่งคลาสที่คะแนนสูงสุด
    label = clf.classes_[idx]              # ชื่อคลาส
    conf = probs[idx] * 100                # แปลงเป็น %
    return label, conf


def on_press(key):
    if key.name == 'B':
        c.blue5.append(req())
        print(f'\nblue5: {len(c.blue5)}')
        fit()
    if key.name == 'W':
        c.white5.append(req())
        print(f'\nwhite5: {len(c.white5)}')
        fit()
    if key.name == 'D':
        c.white2.append(req())
        print(f'\nwhite2: {len(c.white2)}')
        fit()
    if key.name == 'M':
        c.mix.append(req())
        print(f'\nmix: {len(c.mix)}')
        fit()


l = keyboard.Listener(on_press=on_press)
l.start()

while True:
    try:
        label, conf = predict()
        print(f'{label:<7} ({conf:5.1f}%)   \r', end='', flush=True)
    except:
        pass
    time.sleep(0.2)
l.join()
