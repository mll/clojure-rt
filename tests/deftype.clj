(defn def-person
  []
  (deftype*
    user/Person ;; name
    Person ;; class.name
    [name age] ;; fields
    ;; :implements [] ;; TODO
    ;; methods TODO
    ))

(def-person)

(def class1 Person)
java.lang.Class

(def person (Person. "John" 22))
(def class2 (.getClass person))
class1
(= class1 class2) ;; Should be true

(defn age-inc
  [person]
  (+ 1 (.age person)))

(defn name-person
  [person]
  (.name person))

(age-inc (Person. "John" 22))
(age-inc person)
(age-inc person)
(age-inc person)

(age-inc (Person. "Edgar" 41))

(.name person)
(.-name (Person. "Edgar" 41))
Person
(.name person)
(name-person person)
(name-person person)
(name-person person)

(let [person (Person. "Edgar" 41)]
  [(.name person) (.name person) (.name person) (.name person)
   (.age person) (.age person) (.age person)])

(def-person)

(def class3 (.getClass (Person. "Edgar" 41)))
(def class4 Person)
(= class3 class4) ;; should be true
(= class3 class2) ;; should be false

(let [x [1 2 3]]
  [(.persistent (.assoc (.asTransient x) 1 :e)) (.assoc x 1 :e) x])
