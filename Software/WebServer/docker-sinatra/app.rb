# encording: UTF-8

require 'sinatra'
require 'sinatra/reloader'
# require 'sinatra-websocket'
require 'active_record'
require 'sinatra/activerecord'
require 'yaml'
require 'json'
require 'eventmachine'
require 'active_support/core_ext/numeric/conversions'

require "./module/result.rb"

# $stdout.sync = true

set :bind, '0.0.0.0'

config = YAML.load_file('db/database.yml')
ActiveRecord::Base.establish_connection(config["development"])

class RawData < ActiveRecord::Base
  self.table_name = "raw_data"
end

class PlayerData < ActiveRecord::Base
  self.table_name = "player_data"
end

configure do
    set :rcvtest, "-"
    set :maxVirus, 5000
end



get '/' do
  erb :index
end

post '/result' do
    settings.rcvtest = params.inspect
    record = RawData.new
    record.name = params[:name].encode('utf-8')
    record.team = params[:team].to_i
    record.difficulty = params[:difficulty].to_i
    record.redPoint = params[:redPoint].to_i
    record.bluePoint = params[:bluePoint].to_i
    record.greenPoint = params[:greenPoint].to_i
    record.hitPoint = params[:hitPoint].to_i
    record.remainingTime = params[:remainingTime].to_i
    record.save

    # スコア計算
    playerData = generateResult(params)

    # PlayerDataのtableに突っ込む
    playerDB = PlayerData.new
    playerDB.name = playerData[:name]
    playerDB.team = playerData[:team]
    playerDB.difficulty = playerData[:difficulty]
    playerDB.score = playerData[:score]
    playerDB.rank = playerData[:rank]
    playerDB.virusnum = playerData[:virusnum]
    playerDB.gameResult = playerData[:gameResult]
    playerDB.redPoint = playerData[:redPoint]
    playerDB.bluePoint = playerData[:bluePoint]
    playerDB.greenPoint = playerData[:greenPoint]
    playerDB.hitPoint = playerData[:hitPoint]
    playerDB.remainingTime = playerData[:remainingTime]
    playerDB.save

    puts "Request:#{settings.rcvtest}"
end

get '/result' do
  @virusSum = settings.maxVirus - PlayerData.sum(:virusnum)
  if (@virusSum < 0) then
    @virusSum = 0
  end
  @player = PlayerData.last
  @player ||= genEmptyResult()

  # Difficuty
  case @player[:difficulty]
  when "Easy" then
    @difImg = "easy"
  when "Normal" then
    @difImg = "normal"
  when "Hard" then
    @difImg = "hard"
  when "Lunatic" then
    @difImg = "lunatic"
  else
    @difImg = "nil"
  end
  # Result
  if @player[:gameResult] == 1 then
    @gameResult = "clear"
  else
    @gameResult = "failed"
  end
  # Rank
  @rank =  @player[:rank].downcase
  # Team
  @team =  @player[:team].downcase


  erb :result
end

get '/ranking' do
  @player = PlayerData.order(score: :desc)
  @player ||=genEmptyResult()
  erb :ranking
end

get '/ranking_rd' do
  @player = PlayerData.order(score: :desc)
  @player ||=genEmptyResult()
  erb :ranking_rd
end

get '/print' do
  @virusSum = settings.maxVirus - PlayerData.sum(:virusnum)
  if (@virusSum < 0) then
    @virusSum = 0
  end
  @player = PlayerData.last
  @player ||= genEmptyResult()
  if @player[:gameResult] == 1 then
    @gameResult = "Clear!!"
  else
    @gameResult = "Failed..."
  end
  erb :print
end

get '/info' do
  @virusSum = settings.maxVirus - PlayerData.sum(:virusnum)
  if (@virusSum < 0) then
    @virusSum = 0
  end
  @virusMax = settings.maxVirus
  @redTeamSum = PlayerData.where(team: "Red").sum(:virusnum)
  @greenTeamSum = PlayerData.where(team: "Green").sum(:virusnum)
  @blueTeamSum = PlayerData.where(team: "Blue").sum(:virusnum)
  erb :info
end

get '/info_rd' do
  @virusSum = settings.maxVirus - PlayerData.sum(:virusnum)
  if (@virusSum < 0) then
    @virusSum = 0
  end
  @virusMax = settings.maxVirus
  @redTeamSum = PlayerData.where(team: "Red").sum(:virusnum)
  @greenTeamSum = PlayerData.where(team: "Green").sum(:virusnum)
  @blueTeamSum = PlayerData.where(team: "Blue").sum(:virusnum)
  erb :info_rd
end

get '/dbdebug' do 
    @record = RawData.all
    @player = PlayerData.all
    erb :dbdebug
end