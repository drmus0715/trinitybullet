FROM ruby:2.6.4-buster

RUN apt-get update -y && apt-get upgrade -y
RUN apt-get install -y sqlite3 libsqlite3-dev
RUN apt-get clean

WORKDIR /app
COPY Gemfile .
RUN bundle install && bundle clean
COPY . /app
EXPOSE 4567
# CMD ["ruby", "create_db.rb"]
CMD ["bundle", "exec", "ruby", "app.rb", "-o", "0.0.0.0", "-p", "4567"]