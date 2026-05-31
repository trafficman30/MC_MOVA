First attempt (Edge wasn't happy)
=================================
Customised bootstrap v3.3.5...for always collapsing menu
@grid-float-breakpoint = @screen-lg + 999999999px   (default is usually @screen-sm-min)
Success! Your configuration has been saved to https://gist.github.com/b62359c9fa4b37c795d07f8a0ace80d7 and can be revisited here at http://getbootstrap.com/customize/?id=b62359c9fa4b37c795d07f8a0ace80d7 for further customization.


Later changed this, bootstrap version at time bootstrap v3.3.7
Have now set et @grid-float-breakpoint = 9999999px

Edge didn't seem to like the @screen-lg part and  9 9's can also cause problems due to limits in settings widths
so just put to large number 9999999px and seems to work so far.


These new css and js files then need copying to root level.