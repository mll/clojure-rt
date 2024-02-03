(defprotocol Sound
  "What sounds does the entity make?"
  (quiet-sound [this] "Quiet sound")
  (loud-sound [this] [this loud] "Loud sound")
  (self [this]))

(defrecord Car [model]
  Sound
  (quiet-sound [this] (str "Quiet burbling of the " model " engine"))
  (loud-sound [this] (str "Loud horn of the " (:model this) ", reaching over 90 dB!"))
  (loud-sound [this loud] (str "LOUD HORN OF THE " (:model this) ", REACHING OVER 9000 dB!"))
  (self [this] this))

(defrecord Mouse [name]
  Sound
  (loud-sound [this] (quiet-sound this))
  (loud-sound [this loud] "really? ...")
  (quiet-sound [this] "...")
  (self [this] this))

(defrecord Alarm [name]
  Sound
  (quiet-sound [this] (str "Quiet beep, confirming that anti-" (:name this) " alarm is working!"))
  (loud-sound [this] (str "Alarm! The level of " name " is too damn high! Open all windows and leave this room!"))
  (loud-sound [this loud] "LOUD ALARM! JUST RUN!")
  (self [this] this))


(let [a (Car. "Ford")
      b (->Mouse "Mickey")
      c (map->Alarm {:name "carbon monoxide"})]
  (println (quiet-sound a))
  (println (loud-sound a))
  (println (loud-sound a a))
  (println (self a))
  (println (quiet-sound b))
  (println (loud-sound b))
  (println (loud-sound b b))
  (println (self b))
  (println (quiet-sound c))
  (println (loud-sound c))
  (println (loud-sound c c))
  (println (self c)))
