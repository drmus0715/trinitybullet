# encording: UTF-8

# module/result.rb
# 2019 ESC-project
# last update: 2019/10/14
# coding by Hiromu Ozawa

# 定数
BASEPOINT = 100000
PLAYTIME = 50
MAXHP = 5
MS_EASY = 1500
MS_NORMAL = 1000
MS_HARD = 600
MS_LUNATIC = 300
PANEL_WEAK = 0
PANEL_SAME = 1
PANEL_STRONG = 2
COEF_S = 0.7
COEF_A = 0.4
COEF_B = 0.2
COEF_C = 0
COEF_CLEAR = 1
COEF_FAILED = 0.6
SCORE_ONEVIRUS = 600
# Rank Bonus: ランク計算時のスコアボーナス(実スコアには影響なし)
RB_EASY = 0
RB_NORMAL = 0
RB_HARD = 0
RB_LUNATIC = 10000


def generateResult (rawData)
    # hash定義、全部データを入れておく
    playerData = {
        name: "undifined",
        team: "Red",
        difficulty: "Easy",
        score: 0,
        rank: "C",
        virusnum: 0,
        gameResult: 0,
        redPoint: 0,
        bluePoint: 0,
        greenPoint: 0,
        hitPoint: 0,
        remainingTime: 0
    }

    # データの移動
    playerData[:name] = rawData[:name]
    playerData[:redPoint] = rawData[:redPoint].to_i
    playerData[:greenPoint] = rawData[:greenPoint].to_i
    playerData[:bluePoint] = rawData[:bluePoint].to_i
    playerData[:hitPoint] = rawData[:hitPoint].to_i
    playerData[:remainingTime] = rawData[:remainingTime].to_i

    case rawData[:team].to_i
    when 1 then
        playerData[:team] = "Red"
        weakPoint = playerData[:bluePoint]
        strongPoint = playerData[:greenPoint]
        samePoint = playerData[:redPoint]
    when 2 then
        playerData[:team] = "Green"
        weakPoint = playerData[:redPoint]
        strongPoint = playerData[:bluePoint]
        samePoint = playerData[:greenPoint]
    when 3 then
        playerData[:team] = "Blue"
        weakPoint = playerData[:greenPoint]
        strongPoint = playerData[:redPoint]
        samePoint = playerData[:bluePoint]
    else
        playerData[:team] = "Err"
    end

    case rawData[:difficulty].to_i
    when 1 then
        playerData[:difficulty] = "Easy"
        maxPanelNum = PLAYTIME * 1000 / MS_EASY
        rankBonus = RB_EASY
    when 2 then
        playerData[:difficulty] = "Normal"
        maxPanelNum = PLAYTIME * 1000 / MS_NORMAL
        rankBonus = RB_NORMAL
    when 3 then
        playerData[:difficulty] = "Hard"
        maxPanelNum = PLAYTIME * 1000 / MS_HARD
        rankBonus = RB_HARD
    when 4 then
        playerData[:difficulty] = "Lunatic"
        maxPanelNum = PLAYTIME * 1000 / MS_LUNATIC
        rankBonus = RB_LUNATIC
    else
        playerData[:difficulty] = "Err"
    end

    # virusnum
    playerData[:virusnum] = weakPoint * PANEL_WEAK
    playerData[:virusnum] += samePoint * PANEL_SAME
    playerData[:virusnum] += strongPoint * PANEL_STRONG

    # Score
    puts(playerData[:score])
    playerData[:score] = playerData[:virusnum] * SCORE_ONEVIRUS

    # gameResult
    puts(playerData[:score])
    ## クリア時ランクボーナスを付与
    rankBonusBuff = 0
    if playerData[:hitPoint] > 0 then # Clear!
        playerData[:gameResult] = 1
        playerData[:score] = playerData[:score] * COEF_CLEAR
        rankBonusBuff = rankBonus
    elsif playerData[:hitPoint] <= 0 then # Failed...
        playerData[:gameResult] = 0
        playerData[:score] = playerData[:score] * COEF_FAILED
        rankBonusBuff = 0
    end

    # rank
    puts(playerData[:score])
    puts(maxPanelNum * SCORE_ONEVIRUS * COEF_S)
    if playerData[:score] + rankBonusBuff >= maxPanelNum * SCORE_ONEVIRUS * COEF_S then
        # S以上
        playerData[:rank] = "S"
    elsif playerData[:score] + rankBonusBuff >= maxPanelNum * SCORE_ONEVIRUS * COEF_A then
        # A以上
        playerData[:rank] = "A"
    elsif playerData[:score] + rankBonusBuff >= maxPanelNum * SCORE_ONEVIRUS * COEF_B then
        # B以上
        playerData[:rank] = "B"
    elsif playerData[:score] + rankBonusBuff >= maxPanelNum * SCORE_ONEVIRUS * COEF_C then
        # C以上
        playerData[:rank] = "C"
    else
        playerData[:rank] = "Err"
    end

    return playerData
end

def genEmptyResult()
    playerData = {
        name: "",
        team: "",
        difficulty: "",
        score: 0,
        rank: "",
        virusnum: 0,
        gameResult: 0,
        redPoint: 0,
        bluePoint: 0,
        greenPoint: 0,
        hitPoint: 0,
        remainingTime: 0
    }
    return playerData
end